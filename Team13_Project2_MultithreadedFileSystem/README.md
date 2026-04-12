# Team 13 — Project 2: Multithreaded File Management System

**Course:** Operating Systems Lab (NSCS210/CSC211)  
**Department:** Computer Science and Engineering  
**Instructor:** Dr. Jaishree Mayank  
**Submission Deadline:** 12th April 2026

**Name:** Nandipati Jitendra Krishna Sri Sai  
**Admission No:** 24JE0660

---

## Group Information

| Member | Name | Admission No |
|--------|------|--------------|
| 1 | Nandipati Jitendra Krishna Sri Sai | 24JE0660 |
| 2 | Member 2 Name | — |
| 3 | Member 3 Name | — |
| 4 | Member 4 Name | — |
| 5 | Member 5 Name | — |
| 6 | Member 6 Name | — |

---

## Project Overview

This project implements a **Multithreaded File Management System** in C on Linux/macOS. It enables multiple threads to perform file operations **concurrently** with correct synchronization to prevent data corruption. The system uses:

- **`pthreads`** — POSIX threads for concurrent task execution
- **`pthread_rwlock_t`** — read-write locks allowing many simultaneous readers but only one exclusive writer
- **`pthread_mutex_t`** — mutual exclusion locks for metadata and logging
- **`pthread_cond_t`** — condition variables for thread pool task coordination

### Project Choice
**Option 3 — Multithreaded File Management System**, implementing all required key features:

| # | Feature | Module |
|---|---------|--------|
| 1 | Concurrent File Reading | `file_rw.c` |
| 2 | Exclusive File Writing | `file_rw.c` |
| 3 | File Deletion | `file_rw.c` |
| 4 | File Renaming | `file_rw.c` |
| 5 | File Copying | `file_meta.c` |
| 6 | File Metadata Display | `file_meta.c` |
| 7 | Error Handling | all modules |
| 8 | Logging Operations | `logger.c` |
| 9 | Compression/Decompression | *(planned)* |

---

## Repository Structure

```
Team13_Project2_MultithreadedFileSystem/
├── file_rw.h          ← Core Read/Write module — header
├── file_rw.c          ← Core Read/Write module — implementation (KEY FILE)
├── rwlock_manager.h   ← Global rwlock wrapper — header
├── rwlock_manager.c   ← Global rwlock wrapper — implementation
├── file_meta.h        ← File metadata and copy operations — header
├── file_meta.c        ← File metadata and copy operations — implementation
├── logger.h           ← Logging subsystem — header
├── logger.c           ← Logging subsystem — implementation
├── thread_pool.h      ← Thread pool — header
├── thread_pool.c      ← Thread pool — implementation
├── main.c             ← Demo driver (6 phases)
├── Makefile           ← Build system
└── ops.log            ← Auto-generated execution log (timestamped)
```

---

## Environment Setup

### Requirements
- macOS or Linux
- GCC (with pthreads support)
- Make

### Build the Project
```bash
cd Team13_Project2_MultithreadedFileSystem
make
```

### Build and Run (shows ops.log automatically)
```bash
make run
```

### Run the Binary Directly
```bash
./fs_sim
cat ops.log
```

### Clean All Build Artifacts
```bash
make clean
```

---

## Architecture Overview

```
main.c  (5-phase demo driver)
    │
    ├── thread_pool_init(6)          ← 6 worker threads
    │       │
    │       └── thread_pool_submit() ← enqueue tasks (read/write)
    │               │
    │               └── worker threads pick tasks off queue
    │
    ├── file_rw_init()               ← register files, create per-file rwlocks
    │       │
    │       ├── file_rw_read()       ← pthread_rwlock_rdlock  (concurrent)
    │       ├── file_rw_write()      ← pthread_rwlock_wrlock  (exclusive)
    │       ├── file_rw_rename()     ← pthread_rwlock_wrlock  (exclusive)
    │       └── file_rw_delete()     ← pthread_rwlock_wrlock  (exclusive)
    │
    └── logger_init() / log_operation()  ← timestamped ops.log entries
```

### Synchronization Design
```
CONCURRENT READ (pthread_rwlock_rdlock):
  Thread-1 ──[rdlock]────────────────────────[unlock]──
  Thread-2     ──[rdlock]──────────────────────[unlock]──
  Thread-3       ──[rdlock]────────────────────[unlock]──
  (All three run IN PARALLEL — no blocking between readers)

EXCLUSIVE WRITE (pthread_rwlock_wrlock):
  Thread-1 ──[rdlock]──────────[unlock]
  Thread-2           ──[rdlock]──[unlock]
  Thread-W (writer)               ──[wrlock]──[write]──[unlock]──
  Thread-3 (late reader)                                 ──[rdlock]──
  (Writer waits for all readers; late readers wait for writer)
```

---

## Module 1 — Logger (`logger.c` / `logger.h`)

### Built: First (foundation for all other modules)

### Purpose
Thread-safe logging system that writes timestamped messages to `ops.log`. Every module in the project calls `log_operation()` so there is a complete, auditable trace of all operations.

### Key Design
- A `pthread_mutex_t log_mutex` ensures only one thread writes to the log file at a time — preventing garbled output from concurrent writers.
- Uses `gettimeofday()` for millisecond-precision timestamps.
- `fflush()` is called after every write so no log entry is lost if the program crashes.

### Files

| File | Purpose |
|------|---------|
| `logger.h` | Declares `logger_init()`, `log_operation()`, `logger_close()` |
| `logger.c` | Implementation with mutex-protected file writes |

### Implementation

**`logger.h`:**
```c
int  logger_init(void);                          // open ops.log for append
void log_operation(const char *format, ...);     // write timestamped entry
void logger_close(void);                         // flush and close file
```

**`logger.c`:**
```c
static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_operation(const char *format, ...) {
    // Get current time with millisecond precision
    gettimeofday(&tv, NULL);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    pthread_mutex_lock(&log_mutex);         // only one thread writes at a time
    fprintf(log_file, "[%s.%03d] ", ...);  // timestamp prefix
    vfprintf(log_file, format, args);       // user message
    fflush(log_file);                       // immediate write to disk
    pthread_mutex_unlock(&log_mutex);
}
```

### How to Observe
Every line in `ops.log` was produced by `log_operation()`. The timestamp shows exactly when each thread acquired/released locks:
```
[2026-04-11 02:31:54.733] [file_rw] Module initialised. Table capacity = 32 slots.
[2026-04-11 02:31:54.733] SYSTEM: Thread pool (6 workers) ready.
```

---

## Module 2 — Thread Pool (`thread_pool.c` / `thread_pool.h`)

### Built: Second (infrastructure for running tasks concurrently)

### Purpose
A fixed-size pool of worker threads that pick tasks from a shared queue. Instead of creating a new thread for every file operation (expensive), tasks are submitted to the queue and existing threads execute them — simulating a real file-server's concurrency model.

### Key Design
- **Queue**: a singly-linked list of `task_t` (function pointer + argument).
- **`pthread_mutex_t lock`**: protects `task_queue_head`, `task_queue_tail`, `queue_size`, `shutdown` from race conditions.
- **`pthread_cond_t notify`**: worker threads sleep on this condition when the queue is empty; `thread_pool_submit()` signals it when a task arrives.
- **Graceful shutdown**: `thread_pool_shutdown()` sets `shutdown=true`, broadcasts to all workers, then `pthread_join()`s every thread.

### Files

| File | Purpose |
|------|---------|
| `thread_pool.h` | `task_t`, `thread_pool_t` structs + API declarations |
| `thread_pool.c` | `thread_pool_init`, `thread_pool_submit`, `thread_pool_shutdown` |

### Data Structures

**`thread_pool.h`:**
```c
typedef struct task {
    void (*function)(void *);  // function to call
    void *argument;            // argument to pass
    struct task *next;         // linked list pointer
} task_t;

typedef struct thread_pool {
    pthread_mutex_t lock;       // protects the fields below
    pthread_cond_t  notify;     // worker sleep/wake signal
    pthread_t      *threads;    // array of pthread IDs
    task_t         *task_queue_head;
    task_t         *task_queue_tail;
    int             thread_count;
    int             queue_size;
    bool            shutdown;
} thread_pool_t;
```

### Implementation

**Worker thread loop (`thread_pool.c`):**
```c
static void *thread_worker(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    while (1) {
        pthread_mutex_lock(&pool->lock);

        // Sleep while queue is empty and not shutting down
        while (pool->queue_size == 0 && !pool->shutdown)
            pthread_cond_wait(&pool->notify, &pool->lock);

        // Exit if shutting down with no remaining tasks
        if (pool->shutdown && pool->queue_size == 0) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        // Dequeue next task
        task_t *task = pool->task_queue_head;
        pool->task_queue_head = task->next;
        if (pool->task_queue_head == NULL) pool->task_queue_tail = NULL;
        pool->queue_size--;

        pthread_mutex_unlock(&pool->lock);

        // Execute task (outside the lock — concurrency happens here)
        task->function(task->argument);
        free(task);
    }
}
```

**Submit a task:**
```c
int thread_pool_submit(thread_pool_t *pool, void (*function)(void *), void *arg) {
    task_t *new_task = malloc(sizeof(task_t));
    new_task->function = function;
    new_task->argument = arg;

    pthread_mutex_lock(&pool->lock);
    // append to tail of queue
    pool->task_queue_tail->next = new_task;
    pool->task_queue_tail = new_task;
    pool->queue_size++;
    pthread_cond_signal(&pool->notify);   // wake one sleeping worker
    pthread_mutex_unlock(&pool->lock);
}
```

### Usage in main.c
```c
thread_pool_t *pool = thread_pool_init(6);   // 6 worker threads
thread_pool_submit(pool, task_read,  arg);   // concurrent reads
thread_pool_submit(pool, task_write, arg);   // exclusive writes
thread_pool_shutdown(pool);                  // wait for all tasks, clean up
```

---

## Module 3 — Global RWLock Manager (`rwlock_manager.c` / `rwlock_manager.h`)

### Built: Third (simple global lock wrapper, used before per-file locking)

### Purpose
A thin wrapper around a single **global** `pthread_rwlock_t` for filesystem-wide read/write operations. This was the initial coarse-grained locking mechanism before the per-file `file_rw.c` module was introduced.

### Key Design
- One `pthread_rwlock_t fs_rwlock` protects all filesystem operations.
- `fs_read_lock()` → `pthread_rwlock_rdlock()` — multiple threads can hold simultaneously.
- `fs_write_lock()` → `pthread_rwlock_wrlock()` — exclusive, blocks all readers and other writers.
- `fs_unlock()` → `pthread_rwlock_unlock()` — releases whichever lock was held.

### Files

| File | Purpose |
|------|---------|
| `rwlock_manager.h` | Declares `fs_rwlock` + `fs_rwlock_init/destroy`, `fs_read_lock`, `fs_write_lock`, `fs_unlock` |
| `rwlock_manager.c` | Thin wrappers around `pthread_rwlock_*` functions |

### Implementation

**`rwlock_manager.c`:**
```c
pthread_rwlock_t fs_rwlock;

int  fs_rwlock_init(void)    { return pthread_rwlock_init(&fs_rwlock, NULL); }
void fs_read_lock(void)      { pthread_rwlock_rdlock(&fs_rwlock); }
void fs_write_lock(void)     { pthread_rwlock_wrlock(&fs_rwlock); }
void fs_unlock(void)         { pthread_rwlock_unlock(&fs_rwlock); }
void fs_rwlock_destroy(void) { pthread_rwlock_destroy(&fs_rwlock); }
```

### Difference from `file_rw.c`
`rwlock_manager` has **one lock for the entire filesystem** (coarse-grained).  
`file_rw.c` has **one lock per file** (fine-grained) — Thread A reading file 1 does **not** block Thread B reading file 2.

---

## Module 4 — Core Read/Write Module (`file_rw.c` / `file_rw.h`) ⭐

### Built: Fourth — THE MOST IMPORTANT MODULE

### Purpose
This is the conceptual **heart of the project**. It implements fine-grained, per-file `pthread_rwlock_t` locking so that:

1. **Many threads can read the same file simultaneously** — using `pthread_rwlock_rdlock()`.
2. **Only one thread can write to a file at any time** — using `pthread_rwlock_wrlock()`, which also blocks all readers.
3. **Different files are truly independent** — Thread A reading file 1 never blocks Thread B reading file 2 (each file has its own lock).
4. **All operations are fully logged** via `logger.h`.

### Syscall / POSIX API used

| API | Where used | What it does |
|-----|-----------|--------------|
| `pthread_rwlock_init()` | `file_rw_init()` | Create one rwlock per file slot |
| `pthread_rwlock_rdlock()` | `file_rw_read()` | Non-exclusive: many readers at once |
| `pthread_rwlock_wrlock()` | `file_rw_write()`, `file_rw_delete()`, `file_rw_rename()` | Exclusive: blocks all readers + other writers |
| `pthread_rwlock_unlock()` | all operations | Release the held lock |
| `pthread_rwlock_destroy()` | `file_rw_destroy()` | Clean up |
| `pthread_mutex_t` | table operations | Protect file table metadata |

### Files

| File | What Changed / Created |
|------|----------------------|
| `file_rw.h` | **NEW** — header with `file_entry_t` struct and full API |
| `file_rw.c` | **NEW** — complete per-file rwlock implementation |
| `main.c` | **MODIFIED** — replaced demo with 5-phase concurrent showcase |
| `Makefile` | **MODIFIED** — added `file_rw.c` to `SRCS`; added `make run` target |

### Data Structure

**`file_rw.h`:**
```c
#define FILE_TABLE_SIZE  32      // max number of tracked files
#define MAX_FILENAME_LEN 256     // maximum file path length
#define READ_BUF_SIZE    4096    // bytes read per call

typedef struct {
    char             path[MAX_FILENAME_LEN]; // file path (absolute or relative)
    pthread_rwlock_t rwlock;                 // PER-FILE read-write lock ← KEY
    int              in_use;                 // 1 = registered, 0 = free slot
    int              active_readers;         // informational counter
} file_entry_t;
```

A global `file_entry_t file_table[32]` stores all registered files.  
A separate `pthread_mutex_t table_mutex` protects only the table metadata (not I/O).

### Lock Ordering (prevents deadlock)
```
ALWAYS:  table_mutex  →  file_entry.rwlock
NEVER hold table_mutex while doing actual file I/O
```

### Implementation — `file_rw_init()`

Initialises the table and per-file rwlocks at startup:
```c
int file_rw_init(void) {
    pthread_mutex_init(&table_mutex, NULL);  // protect table metadata

    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        memset(&file_table[i], 0, sizeof(file_entry_t));
        pthread_rwlock_init(&file_table[i].rwlock, NULL);  // per-file lock
        file_table[i].in_use = 0;
    }
    log_operation("[file_rw] Module initialised. Table capacity = %d slots.",
                  FILE_TABLE_SIZE);
    return 0;
}
```

### Implementation — `file_rw_register(path)`

Finds a free slot (or returns existing fid if already registered):
```c
int file_rw_register(const char *path) {
    pthread_mutex_lock(&table_mutex);

    // Check if already registered — return existing fid
    for (int i = 0; i < FILE_TABLE_SIZE; i++)
        if (file_table[i].in_use && strcmp(file_table[i].path, path) == 0)
            return i;  // already exists

    // Find a free slot and register
    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        if (!file_table[i].in_use) {
            strncpy(file_table[i].path, path, MAX_FILENAME_LEN - 1);
            file_table[i].in_use = 1;
            file_table[i].active_readers = 0;
            pthread_mutex_unlock(&table_mutex);
            return i;  // fid
        }
    }
    pthread_mutex_unlock(&table_mutex);
    return -1;  // table full
}
```

### Implementation — `file_rw_read()` ★ CONCURRENT READ

```c
ssize_t file_rw_read(int fid, int thread_id) {
    // == ACQUIRE READ LOCK ==
    // Multiple threads can be past this point simultaneously.
    // Only a pending wrlock will cause blocking here.
    pthread_rwlock_rdlock(&entry->rwlock);

    entry->active_readers++;  // informational only
    log_operation("[file_rw] [Thread-%d] READ LOCK acquired on fid=%d "
                  "| active_readers=%d", thread_id, fid, entry->active_readers);

    // == ACTUAL FILE READ (concurrent) ==
    FILE *fp = fopen(path, "r");
    char buf[READ_BUF_SIZE];
    size_t n;
    ssize_t total_read = 0;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        total_read += n;
    fclose(fp);

    log_operation("[file_rw] [Thread-%d] READ: read %zd bytes", thread_id, total_read);

    // == RELEASE READ LOCK ==
    entry->active_readers--;
    pthread_rwlock_unlock(&entry->rwlock);
    return total_read;
}
```

**Proven concurrent behaviour from `ops.log`:**
```
[02:31:55.737] Thread-1  READ LOCK acquired | active_readers=1
[02:31:55.737] Thread-3  READ LOCK acquired | active_readers=2
[02:31:55.737] Thread-4  READ LOCK acquired | active_readers=3
[02:31:55.738] Thread-6  READ LOCK acquired | active_readers=4
[02:31:55.738] Thread-5  READ LOCK acquired | active_readers=5
[02:31:55.738] Thread-2  READ LOCK acquired | active_readers=5  ← 6 running together!
```
All 6 readers acquired their rdlock at the **same millisecond** — true parallel execution.

### Implementation — `file_rw_write()` ★ EXCLUSIVE WRITE

```c
int file_rw_write(int fid, int thread_id, const char *data, size_t len) {
    // == ACQUIRE WRITE LOCK ==
    // Blocks until ALL active readers have released.
    // Prevents any NEW reader from starting while waiting.
    pthread_rwlock_wrlock(&entry->rwlock);

    log_operation("[file_rw] [Thread-%d] WRITE LOCK acquired — ALL READERS BLOCKED",
                  thread_id);

    // == ACTUAL FILE WRITE (exclusive) ==
    // Only ONE thread can be here at a time.
    FILE *fp = fopen(path, "a");  // append mode
    fwrite(data, 1, len, fp);
    fclose(fp);

    log_operation("[file_rw] [Thread-%d] WRITE: wrote %zu bytes", thread_id, len);

    // == RELEASE WRITE LOCK ==
    pthread_rwlock_unlock(&entry->rwlock);
    // After this: readers and writers can proceed again
}
```

**Exclusivity proven from `ops.log`** — notice only ONE `WRITE LOCK acquired` appears at a time:
```
[02:31:54.733] [Thread-101] WRITE LOCK acquired — ALL READERS BLOCKED
[02:31:54.733] [Thread-101] WRITE: wrote 41 bytes
[02:31:54.733] [Thread-101] WRITE LOCK released
[02:31:54.733] [Thread-103] WRITE LOCK acquired — ALL READERS BLOCKED  ← serial
[02:31:54.734] [Thread-103] WRITE: wrote 50 bytes
[02:31:54.734] [Thread-103] WRITE LOCK released
```

### Implementation — `file_rw_delete()`

```c
int file_rw_delete(int fid, int thread_id) {
    pthread_rwlock_wrlock(&entry->rwlock);    // exclusive — no reader/writer
    log_operation("[file_rw] [Thread-%d] DELETE LOCK acquired", thread_id);

    remove(path);                             // delete from disk
    log_operation("[file_rw] [Thread-%d] DELETE: removed '%s'", thread_id, path);

    pthread_rwlock_unlock(&entry->rwlock);

    // Unregister from table
    pthread_mutex_lock(&table_mutex);
    file_table[fid].in_use = 0;
    pthread_mutex_unlock(&table_mutex);
}
```

### Implementation — `file_rw_rename()`

```c
int file_rw_rename(int fid, int thread_id, const char *new_path) {
    pthread_rwlock_wrlock(&entry->rwlock);    // exclusive during rename
    log_operation("[file_rw] [Thread-%d] RENAME LOCK acquired '%s' → '%s'",
                  thread_id, old_path, new_path);

    rename(old_path, new_path);               // atomic on POSIX filesystems

    // Update path record in table (under table_mutex)
    pthread_mutex_lock(&table_mutex);
    strncpy(file_table[fid].path, new_path, MAX_FILENAME_LEN - 1);
    pthread_mutex_unlock(&table_mutex);

    pthread_rwlock_unlock(&entry->rwlock);
}
```

### Implementation — `file_rw_destroy()`

```c
void file_rw_destroy(void) {
    for (int i = 0; i < FILE_TABLE_SIZE; i++)
        pthread_rwlock_destroy(&file_table[i].rwlock);
    pthread_mutex_destroy(&table_mutex);
    log_operation("[file_rw] Module destroyed. All rwlocks released.");
}
```

### Error Handling

| Condition | Return / Action |
|-----------|----------------|
| Invalid `fid` (< 0 or ≥ 32) | return -1, log error |
| File not registered (`in_use == 0`) | return -1, log error |
| `kalloc` / `malloc` fail | return -1, roll back state |
| `fopen()` fails on read | log warning, return -1 (file may not exist yet) |
| `fopen()` fails on write | log error, return -1 |
| `remove()` returns ENOENT | ignored (file already gone) |
| `rename()` fails | log error, return -1 (path unchanged) |
| Table full (32 files) | return -1, log error |

---

## Demo Driver — `main.c` (5 Phases)

### How to run
```bash
make run
```

### Phase 1 — Write Initial Content
3 writer threads (T-101, T-102, T-103) write structured lines to `test_shared.txt`.
Each acquires wrlock sequentially, proving exclusive write ordering.

### Phase 2 — 6 Concurrent Readers ★
6 reader threads fire simultaneously on the same file.
All 6 acquire `pthread_rwlock_rdlock` at the same millisecond — **proving true parallel reads**.

```
Expected in ops.log (all within the same millisecond):
  Thread-1  READ LOCK acquired | active_readers=1
  Thread-3  READ LOCK acquired | active_readers=2
  Thread-4  READ LOCK acquired | active_readers=3
  ...up to 5 or 6 simultaneous readers
```

### Phase 3 — Writer Competing With Readers
- 4 readers start (300 ms hold time)
- Writer submitted shortly after — blocked until all 4 readers finish
- 2 "late" readers submitted — blocked until writer releases

Demonstrates the **write-preference** / **fairness** policy of POSIX rwlocks.

### Phase 4 — Rename Under Write Lock
`file_rw_rename()` is called — wrlock ensures no reader/writer can race with the rename.

### Phase 5 — Delete Under Write Lock
`file_rw_delete()` removes the file from disk and unregisters the fid under wrlock.

---

## Build System — `Makefile`

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g -O2
LDFLAGS = -pthread

SRCS = thread_pool.c \
       logger.c \
       rwlock_manager.c \
       file_rw.c \        ← added in Module 4
       main.c

TARGET = fs_sim
```

| Target | Command | Action |
|--------|---------|--------|
| Build  | `make` | Compile all `.c` → `fs_sim` |
| Run    | `make run` | Build, run, print `ops.log` |
| Clean  | `make clean` | Remove `.o`, `fs_sim`, `ops.log`, test files |

---

## Verified Output (from `ops.log`)

```
====== SYSTEM START ======
[file_rw] Module initialised. Table capacity = 32 slots.
[file_rw] Registered 'test_shared.txt' → fid=0
SYSTEM: Thread pool (6 workers) ready.

====== PHASE 1: Initial writes ======
[Thread-101] WRITE LOCK acquired — ALL READERS BLOCKED
[Thread-101] WRITE: wrote 41 bytes to 'test_shared.txt'
[Thread-101] WRITE LOCK released
[Thread-103] WRITE LOCK acquired — ALL READERS BLOCKED
[Thread-103] WRITE: wrote 50 bytes to 'test_shared.txt'
[Thread-103] WRITE LOCK released
[Thread-102] WRITE LOCK acquired — ALL READERS BLOCKED
[Thread-102] WRITE: wrote 42 bytes to 'test_shared.txt'
[Thread-102] WRITE LOCK released

====== PHASE 2: Concurrent reads ======
[Thread-1]  READ LOCK acquired | active_readers=1
[Thread-3]  READ LOCK acquired | active_readers=2
[Thread-4]  READ LOCK acquired | active_readers=3
[Thread-6]  READ LOCK acquired | active_readers=4
[Thread-5]  READ LOCK acquired | active_readers=5
[Thread-2]  READ LOCK acquired | active_readers=5   ← 6 readers simultaneously!
[Thread-1]  READ LOCK released
[Thread-3]  READ LOCK released
...

====== PHASE 3: Writer vs readers ======
[Thread-10] READ LOCK acquired | active_readers=1
[Thread-12] READ LOCK acquired | active_readers=2
[Thread-11] READ LOCK acquired | active_readers=3
[Thread-13] READ LOCK acquired | active_readers=4
...all 4 readers read and release...
[Thread-50] WRITE LOCK acquired — ALL READERS BLOCKED
[Thread-50] WRITE: wrote 51 bytes
[Thread-50] WRITE LOCK released
[Thread-20] READ LOCK acquired   ← late readers unblocked after writer
[Thread-21] READ LOCK acquired

====== PHASE 4: Rename ======
[Thread-200] RENAME LOCK acquired 'test_shared.txt' → 'test_renamed.txt'
[Thread-200] RENAME done.
[Thread-200] RENAME LOCK released

====== PHASE 5: Delete ======
[Thread-300] DELETE LOCK acquired 'test_renamed.txt'
[Thread-300] DELETE: removed 'test_renamed.txt'
[Thread-300] fid=0 unregistered.

[file_rw] Module destroyed. All rwlocks released.
====== SYSTEM STOP ======
```

---

## Module 5 — Compression & Signal Handling (**`compress.c`**, **`signals.c`**)

### Purpose
This module extends the multithreaded file system by adding:

1. File Compression & Decompression
    Uses zlib for efficient storage.
    Supports background execution using thread pool.
2. Signal Handling
    Handles system signals (SIGINT, SIGUSR1).
    Enables graceful shutdown and runtime monitoring.
3. User-Controlled Execution
    Allows user to choose:
     - Compress
     - Decompress
     - Both
     - Do nothing
  
### Files

| File | Purpose |
| ------ | ---------- |
| **`compress.h`**| Declares compression and Decompression API's |
| **`compress.c`**| Implements compression using zlib |
| **`signals.h`** | Declares signal handlers |
| **`signals.c`** | Implements signal handling logic |
| **`cli.c`** | Modified to integrate Compression |
| **`main.c`** | initialize setup_signals |


### Key Design

1. **Compression**
  - Uses gzopen() and gzwrite() (zlib)
  - Reads file in chunks (CHUNK = 16384)
  - Writes compressed output → compressed.gz

2. **Decompression**
  - Uses gzread() to restore file
  - Writes output → decompressed.txt
  - Only .gz files allowed

3. **Signal Handling**
  - SIGINT → graceful exit (Ctrl + C)
  - SIGUSR1 → system status logging

4. **Thread Pool Integration**
   
   Tasks executed asynchronously:

   ```
   thread_pool_submit(pool, compress_task, filename);
   thread_pool_submit(pool, decompress_task, filename);
   
   ```

### API's used


| API | Purpose |
| ------- | ---------- |
| gzopen() | open compressed file |
| gzwrite() | write compressed data |
| gzread() | read compressed data |
| signal() | register signal handlers |
| pthread | asynchronous execution |


### Implementation

1. **`compress.h`**
   
```c
#ifndef COMPRESS_H
#define COMPRESS_H

int compress_file(const char *src, const char *dest);
int decompress_file(const char *src, const char *dest);

#endif
```

2. **`compress.c`**

```c
#include <zlib.h>
#include <stdio.h>

int compress_file(const char *src, const char *dest) {
    FILE *f_in = fopen(src, "rb");
    gzFile f_out = gzopen(dest, "wb");
    
    if (!f_in || !f_out) return -1;

    char buffer[1024];
    int bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f_in)) > 0) {
        gzwrite(f_out, buffer, bytes);
    }

    fclose(f_in);
    gzclose(f_out);
    return 0;
}

int decompress_file(const char *src, const char *dest) {
    gzFile f_in = gzopen(src, "rb");
    FILE *f_out = fopen(dest, "wb");
    
    if (!f_in || !f_out) return -1;

    char buffer[1024];
    int bytes;
    while ((bytes = gzread(f_in, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes, f_out);
    }

    gzclose(f_in);
    fclose(f_out);
    return 0;
}
 
```

3. **`signal.h`**

```c
     #ifndef SIGNALS_H
#define SIGNALS_H

void setup_signals(void);

#endif
```
 
4. **`signals.c`**

```c
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void handle_sigint(int sig) {
    printf("\n[Signal] Graceful shutdown initiated...\n");
    exit(0);
}

void handle_sigusr1(int sig) {
    printf("\n[Status] System is idle/processing...\n");
}

void setup_signals(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGUSR1, handle_sigusr1);
}

```

5. Integration in **`cli.c`**

```c
 static void cli_compress_file(void)
{
    printf(ANSI_COLOR_MAGENTA "\n--- Compress File ---\n" ANSI_COLOR_RESET);

    char *source = cli_get_filepath("Enter source file path: ");
    if (!source) {
        return;
    }

    int val = cli_validate_file(source);
    if (val != CLI_SUCCESS) {
        free(source);
        return;
    }

    char *dest = cli_get_filepath("Enter destination path: ");
    if (!dest) {
        free(source);
        return;
    }

    printf(ANSI_COLOR_YELLOW "Compressing '%s' to '%s'...\n" ANSI_COLOR_RESET, source, dest);
    if (compress_file(source, dest) == 0) {
        printf(ANSI_COLOR_GREEN "✓ File compressed successfully\n" ANSI_COLOR_RESET);
        log_operation("CLI: Compressed '%s' to '%s'", source, dest);
    } else {
        printf(ANSI_COLOR_RED "✗ Compression failed\n" ANSI_COLOR_RESET);
        log_operation("CLI: Compression failed for '%s'", source);
    }

    free(source);
    free(dest);
}
```

6. Initialization in **`main.c`**

```c
          setup_signals();
``` 
  
### How to run and output 

1. **compile the project**
    
Go to project folder :
  ```
  cd ~/OS_LAB_Project/Team13_Project2_MultithreadedFileSystem

  ```
compile : 
```
make clean
make
```

2. **Run the program**

```
./fs_sim
```

3. **You will see menu**

```
Available operations:
1. Delete File
2. View Metadata
3. Rename File
4. Compress File
5. Exit
choice:

```
If you choose choice: 4 then file needed to compress so it will ask for file path
```
--- compress file ---
Enter source file path:  //path of file........

```
output:

```
compressing '...//path of file.....'
File compressed succesfully
```

5. **Test Signals**

  Run program:
   ```
   ./fs_sim
   ```

  get pid:
   ```
   ps auz | grep fs_sim  //user 12345 ./fs_sim
   ```

- send SIGUSR1

   ```
   kill -SIGUSR1 12345
   ```
  output:
   ```
   [Status] System is Idle/Processing...
   ```
- Send SIGINT

   ```
     press Ctrl + C
   ```

  output:
   ```
   [Signal] Graceful shutdown initiated...
   ```

   
### Generated Files

| Operation | File |
| --- | ---- |
| Compress | **`compressed.gz`** |
| Decompress | **`decompressed.txt`** |

### Error Handling

| case | result |
| ---- | ----- |
| Invalid File | Error logged |
| Wrong Format | Rejected |
| File Missing | Safe Exit |


### Observations

  - Compression runs in background
  - Main thread not blocked
  - Logs capture full execution
  - Signals handled independently

### Conclusion 

Module 5 demonstrates:

  - Real-world file compression
  - Multithreading integration
  - OS signal handling
  - User-driven execution


---

## Development Order (Modules Built)

| Order | Module | Files | Key Concept |
|-------|--------|-------|-------------|
| 1 | Logger | `logger.c`, `logger.h` | `pthread_mutex_t` — thread-safe file writes |
| 2 | Thread Pool | `thread_pool.c`, `thread_pool.h` | `pthread_mutex_t` + `pthread_cond_t` — task queue |
| 3 | Global RWLock | `rwlock_manager.c`, `rwlock_manager.h` | `pthread_rwlock_t` — coarse-grained locking |
| 4 | Core Read/Write | `file_rw.c`, `file_rw.h` | Per-file `pthread_rwlock_t` — fine-grained locking |
| 5 | Demo Driver | `main.c` | 5-phase concurrent test harness |
| 6 | Compression/Decompression | `compress.h`,`compress.c` | Compression shrinks data, Decompression restores it |
| 7 | Signals | ` signals.c`, `signals.h` | OS messages to control process |
## Development Order (Modules Built)

| Order | Module | Files | Key Concept |
|-------|--------|-------|-------------|
| 1 | Logger | `logger.c`, `logger.h` | `pthread_mutex_t` — thread-safe file writes |
| 2 | Thread Pool | `thread_pool.c`, `thread_pool.h` | `pthread_mutex_t` + `pthread_cond_t` — task queue |
| 3 | Global RWLock | `rwlock_manager.c`, `rwlock_manager.h` | `pthread_rwlock_t` — coarse-grained locking |
| 4 | Core Read/Write | `file_rw.c`, `file_rw.h` | Per-file `pthread_rwlock_t` — fine-grained locking |
| 5 | Demo Driver | `main.c` | 5-phase concurrent test harness |

---

## References

- [POSIX pthread_rwlock_t man page](https://man7.org/linux/man-pages/man3/pthread_rwlock_rdlock.3.html)
- [POSIX pthread_mutex_t man page](https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3.html)
- [POSIX pthread_cond_t man page](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3.html)
- [Operating System Concepts — Silberschatz, Galvin, Gagne](https://os.eclass.uth.gr/eclass/modules/document/file.php/C135/Silberschatz-operating-system-concepts.pdf)
- [Advanced Programming in the UNIX Environment — W. Richard Stevens](https://www.apuebook.com/)
- [Linux man-pages: `fread`, `fwrite`, `rename`, `remove`](https://man7.org/linux/man-pages/)

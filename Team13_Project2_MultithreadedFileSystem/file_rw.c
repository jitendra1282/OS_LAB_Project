/*
 * file_rw.c — Core Read/Write Module for Multithreaded File System
 *
 * KEY CONCEPTS DEMONSTRATED
 * ══════════════════════════════════════════════════════════════
 *  1. CONCURRENT READS
 *       pthread_rwlock_rdlock() is acquired by EVERY reader thread.
 *       The POSIX rwlock allows unlimited concurrent readers as long
 *       as NO writer holds (or is waiting for) the lock.
 *       → Multiple threads can read the same file in parallel.
 *
 *  2. EXCLUSIVE WRITES
 *       pthread_rwlock_wrlock() blocks until ALL active readers have
 *       released AND no other writer holds the lock.
 *       → Only one thread can write at any moment; readers are also
 *         blocked while a write is in progress.
 *
 *  3. PER-FILE LOCKS
 *       Each file entry has its OWN pthread_rwlock_t.
 *       Thread A reading file 1 does NOT block Thread B reading file 2.
 *
 *  4. TABLE-LEVEL MUTEX
 *       A separate pthread_mutex_t (table_mutex) protects only the
 *       file_table[] array metadata (in_use, path, active_readers).
 *       It is held only during registration/lookup — not during I/O.
 *
 * LOCK ORDERING (to prevent deadlock)
 *   Always: table_mutex → file_entry.rwlock
 *   Never hold table_mutex while doing I/O.
 *
 * All operations are logged to ops.log via logger.h.
 */

#include "file_rw.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

/* ── internal file table ────────────────────────────────────── */
static file_entry_t  file_table[FILE_TABLE_SIZE];
static pthread_mutex_t table_mutex;   /* protects the table metadata */
static int           module_ready = 0;

/* ── helpers ────────────────────────────────────────────────── */

/* Return locked table entry for fid, or NULL if invalid. */
static file_entry_t *get_entry(int fid)
{
    if (fid < 0 || fid >= FILE_TABLE_SIZE)
        return NULL;
    if (!file_table[fid].in_use)
        return NULL;
    return &file_table[fid];
}

/* ── public API ─────────────────────────────────────────────── */

/*
 * file_rw_init()
 *   Zero-initialises the file table and creates the table_mutex.
 *   Also initialises every per-file rwlock so they are ready to use.
 */
int file_rw_init(void)
{
    if (pthread_mutex_init(&table_mutex, NULL) != 0) {
        log_operation("[file_rw] ERROR: failed to init table_mutex: %s",
                      strerror(errno));
        return -1;
    }

    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        memset(&file_table[i], 0, sizeof(file_entry_t));
        if (pthread_rwlock_init(&file_table[i].rwlock, NULL) != 0) {
            log_operation("[file_rw] ERROR: failed to init rwlock[%d]: %s",
                          i, strerror(errno));
            return -1;
        }
        file_table[i].in_use = 0;
    }

    module_ready = 1;
    log_operation("[file_rw] Module initialised. Table capacity = %d slots.",
                  FILE_TABLE_SIZE);
    return 0;
}

/*
 * file_rw_register(path)
 *   Find a free slot in the table, copy the path, mark it as in_use.
 *   If path is already registered, return its existing fid.
 */
int file_rw_register(const char *path)
{
    if (!module_ready || !path) return -1;

    pthread_mutex_lock(&table_mutex);

    /* Check if already registered */
    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        if (file_table[i].in_use &&
            strncmp(file_table[i].path, path, MAX_FILENAME_LEN) == 0) {
            pthread_mutex_unlock(&table_mutex);
            log_operation("[file_rw] Already registered: '%s' → fid=%d", path, i);
            return i;
        }
    }

    /* Find free slot */
    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        if (!file_table[i].in_use) {
            strncpy(file_table[i].path, path, MAX_FILENAME_LEN - 1);
            file_table[i].path[MAX_FILENAME_LEN - 1] = '\0';
            file_table[i].in_use         = 1;
            file_table[i].active_readers = 0;
            pthread_mutex_unlock(&table_mutex);
            log_operation("[file_rw] Registered '%s' → fid=%d", path, i);
            return i;
        }
    }

    pthread_mutex_unlock(&table_mutex);
    log_operation("[file_rw] ERROR: file table full, cannot register '%s'", path);
    return -1;
}

/*
 * ══════════════════════════════════════════════════════════════
 * file_rw_read(fid, thread_id)
 *
 * CONCURRENT READ — This is the core demonstration.
 *
 * pthread_rwlock_rdlock() is POSIX read-lock:
 *   • Succeeds immediately if NO writer holds the lock.
 *   • Multiple callers can all hold it at the same time.
 *   • Blocks only when a writer is active.
 *
 * Execution timeline (3 reader threads, 1 writer):
 *
 *  Time →
 *  Thread-1  [rdlock]=====[reading]=====[unlock]
 *  Thread-2     [rdlock]=====[reading]=====[unlock]
 *  Thread-3        [rdlock]=====[reading]=====[unlock]
 *  Thread-4 (W)               [--- wrlock blocks ---][writing][unlock]
 *
 *  Threads 1-3 run in parallel. Writer cannot start until ALL readers
 *  have unlocked.
 * ══════════════════════════════════════════════════════════════
 */
ssize_t file_rw_read(int fid, int thread_id)
{
    pthread_mutex_lock(&table_mutex);
    file_entry_t *entry = get_entry(fid);
    if (!entry) {
        pthread_mutex_unlock(&table_mutex);
        log_operation("[file_rw] [Thread-%d] READ ERROR: invalid fid=%d",
                      thread_id, fid);
        return -1;
    }
    /* Copy path locally so we can release table_mutex before I/O */
    char path[MAX_FILENAME_LEN];
    strncpy(path, entry->path, MAX_FILENAME_LEN);
    pthread_mutex_unlock(&table_mutex);

    /* ── ACQUIRE READ LOCK ────────────────────────────────────
     * Multiple threads can be past this point simultaneously.
     * Only a pending wrlock will make this block.
     * ─────────────────────────────────────────────────────── */
    pthread_rwlock_rdlock(&entry->rwlock);

    /* Informational: bump reader counter (guarded by rwlock itself here) */
    __sync_fetch_and_add(&entry->active_readers, 1);

    log_operation("[file_rw] [Thread-%d] READ LOCK acquired on fid=%d ('%s') "
                  "| active_readers=%d",
                  thread_id, fid, path, entry->active_readers);

    /* ── ACTUAL FILE READ ─────────────────────────────────── */
    FILE *fp = fopen(path, "r");
    ssize_t total_read = 0;

    if (!fp) {
        log_operation("[file_rw] [Thread-%d] READ WARN: cannot open '%s': %s "
                      "(file may not exist yet)",
                      thread_id, path, strerror(errno));
    } else {
        char buf[READ_BUF_SIZE];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
            total_read += (ssize_t)n;
        }
        fclose(fp);
        log_operation("[file_rw] [Thread-%d] READ: read %zd bytes from '%s'",
                      thread_id, total_read, path);
    }

    /* ── RELEASE READ LOCK ────────────────────────────────── */
    __sync_fetch_and_sub(&entry->active_readers, 1);
    pthread_rwlock_unlock(&entry->rwlock);

    log_operation("[file_rw] [Thread-%d] READ LOCK released on fid=%d",
                  thread_id, fid);

    return total_read;
}

/*
 * ══════════════════════════════════════════════════════════════
 * file_rw_write(fid, thread_id, data, len)
 *
 * EXCLUSIVE WRITE — Only ONE thread may write at a time.
 *
 * pthread_rwlock_wrlock():
 *   • Blocks until ALL active readers have released their rdlocks.
 *   • Blocks if another writer holds the lock.
 *   • Once acquired, PREVENTS any new rdlock from succeeding.
 *
 * In POSIX, the implementation may give priority to writers to
 * prevent "writer starvation" while many readers keep arriving.
 * ══════════════════════════════════════════════════════════════
 */
int file_rw_write(int fid, int thread_id, const char *data, size_t len)
{
    pthread_mutex_lock(&table_mutex);
    file_entry_t *entry = get_entry(fid);
    if (!entry) {
        pthread_mutex_unlock(&table_mutex);
        log_operation("[file_rw] [Thread-%d] WRITE ERROR: invalid fid=%d",
                      thread_id, fid);
        return -1;
    }
    char path[MAX_FILENAME_LEN];
    strncpy(path, entry->path, MAX_FILENAME_LEN);
    pthread_mutex_unlock(&table_mutex);

    /* ── ACQUIRE WRITE LOCK ───────────────────────────────────
     * Blocks until ALL readers have unlocked AND no other writer
     * is active. This is the "exclusive" guarantee.
     * ─────────────────────────────────────────────────────── */
    pthread_rwlock_wrlock(&entry->rwlock);

    log_operation("[file_rw] [Thread-%d] WRITE LOCK acquired on fid=%d ('%s') "
                  "— ALL READERS BLOCKED",
                  thread_id, fid, path);

    /* ── ACTUAL FILE WRITE ───────────────────────────────────
     * Append mode: each write call adds to the end of the file.
     * Only one thread is in this critical section at a time.
     * ─────────────────────────────────────────────────────── */
    FILE *fp = fopen(path, "a");
    int ret = 0;

    if (!fp) {
        log_operation("[file_rw] [Thread-%d] WRITE ERROR: cannot open '%s': %s",
                      thread_id, path, strerror(errno));
        ret = -1;
    } else {
        size_t written = fwrite(data, 1, len, fp);
        fprintf(fp, "\n");          /* newline after each write */
        fclose(fp);
        if (written != len) {
            log_operation("[file_rw] [Thread-%d] WRITE WARN: wrote %zu/%zu bytes to '%s'",
                          thread_id, written, len, path);
            ret = -1;
        } else {
            log_operation("[file_rw] [Thread-%d] WRITE: wrote %zu bytes to '%s'",
                          thread_id, len, path);
        }
    }

    /* ── RELEASE WRITE LOCK ──────────────────────────────────
     * After this point readers and other writers can proceed.
     * ─────────────────────────────────────────────────────── */
    pthread_rwlock_unlock(&entry->rwlock);

    log_operation("[file_rw] [Thread-%d] WRITE LOCK released on fid=%d",
                  thread_id, fid);

    return ret;
}

/*
 * file_rw_delete(fid, thread_id)
 *   Exclusively lock, remove from disk, unregister the fid.
 *   Uses wrlock because deletion must not race with readers or writers.
 */
int file_rw_delete(int fid, int thread_id)
{
    pthread_mutex_lock(&table_mutex);
    file_entry_t *entry = get_entry(fid);
    if (!entry) {
        pthread_mutex_unlock(&table_mutex);
        log_operation("[file_rw] [Thread-%d] DELETE ERROR: invalid fid=%d",
                      thread_id, fid);
        return -1;
    }
    char path[MAX_FILENAME_LEN];
    strncpy(path, entry->path, MAX_FILENAME_LEN);
    pthread_mutex_unlock(&table_mutex);

    /* Exclusive lock — no reader or writer may proceed */
    pthread_rwlock_wrlock(&entry->rwlock);
    log_operation("[file_rw] [Thread-%d] DELETE LOCK acquired on fid=%d ('%s')",
                  thread_id, fid, path);

    int ret = 0;
    if (remove(path) != 0) {
        if (errno != ENOENT) {          /* ignore "file not found" */
            log_operation("[file_rw] [Thread-%d] DELETE ERROR: remove('%s'): %s",
                          thread_id, path, strerror(errno));
            ret = -1;
        }
    }

    if (ret == 0) {
        log_operation("[file_rw] [Thread-%d] DELETE: removed '%s'",
                      thread_id, path);
    }

    pthread_rwlock_unlock(&entry->rwlock);

    /* Remove from table */
    pthread_mutex_lock(&table_mutex);
    file_table[fid].in_use = 0;
    memset(file_table[fid].path, 0, MAX_FILENAME_LEN);
    pthread_mutex_unlock(&table_mutex);

    log_operation("[file_rw] [Thread-%d] fid=%d unregistered.", thread_id, fid);
    return ret;
}

/*
 * file_rw_rename(fid, thread_id, new_path)
 *   Exclusively lock, rename on disk, update internal path record.
 */
int file_rw_rename(int fid, int thread_id, const char *new_path)
{
    if (!new_path) return -1;

    pthread_mutex_lock(&table_mutex);
    file_entry_t *entry = get_entry(fid);
    if (!entry) {
        pthread_mutex_unlock(&table_mutex);
        log_operation("[file_rw] [Thread-%d] RENAME ERROR: invalid fid=%d",
                      thread_id, fid);
        return -1;
    }
    char old_path[MAX_FILENAME_LEN];
    strncpy(old_path, entry->path, MAX_FILENAME_LEN);
    pthread_mutex_unlock(&table_mutex);

    pthread_rwlock_wrlock(&entry->rwlock);
    log_operation("[file_rw] [Thread-%d] RENAME LOCK acquired on fid=%d ('%s' → '%s')",
                  thread_id, fid, old_path, new_path);

    int ret = 0;
    if (rename(old_path, new_path) != 0) {
        log_operation("[file_rw] [Thread-%d] RENAME ERROR: rename('%s','%s'): %s",
                      thread_id, old_path, new_path, strerror(errno));
        ret = -1;
    } else {
        /* Update path in table */
        pthread_mutex_lock(&table_mutex);
        strncpy(file_table[fid].path, new_path, MAX_FILENAME_LEN - 1);
        file_table[fid].path[MAX_FILENAME_LEN - 1] = '\0';
        pthread_mutex_unlock(&table_mutex);

        log_operation("[file_rw] [Thread-%d] RENAME: '%s' → '%s' done.",
                      thread_id, old_path, new_path);
    }

    pthread_rwlock_unlock(&entry->rwlock);
    log_operation("[file_rw] [Thread-%d] RENAME LOCK released on fid=%d",
                  thread_id, fid);
    return ret;
}

/*
 * file_rw_destroy()
 *   Destroy every rwlock and the table mutex.
 *   Must be called after all threads have finished.
 */
void file_rw_destroy(void)
{
    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        pthread_rwlock_destroy(&file_table[i].rwlock);
    }
    pthread_mutex_destroy(&table_mutex);
    module_ready = 0;
    log_operation("[file_rw] Module destroyed. All rwlocks released.");
}

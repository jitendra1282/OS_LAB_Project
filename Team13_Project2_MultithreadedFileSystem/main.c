/*
 * main.c — Multithreaded File Management System — Demo Driver
 *
 * This program demonstrates the core read/write module (file_rw.c):
 *
 *   PHASE 1 — WRITE SETUP
 *     3 sequential writes to a test file so it has content to read.
 *
 *   PHASE 2 — CONCURRENT READS (core demonstration)
 *     6 reader threads all call file_rw_read() on THE SAME file.
 *     They all acquire pthread_rwlock_rdlock and run IN PARALLEL.
 *     The ops.log shows all 6 "READ LOCK acquired" lines before any
 *     "READ LOCK released" — proving simultaneous reads.
 *
 *   PHASE 3 — EXCLUSIVE WRITE DURING READS
 *     1 writer thread competes with 4 reader threads.
 *     The writer (pthread_rwlock_wrlock) must wait for all readers
 *     to finish. Readers started AFTER the writer's attempt will
 *     also block until the write completes.
 *
 *   PHASE 4 — RENAME & DELETE
 *     Shows file operations under exclusive write locks.
 *
 * All activity is logged to ops.log with timestamps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "file_rw.h"
#include "logger.h"
#include "thread_pool.h"

/* ── shared state ───────────────────────────────────────────── */
#define TEST_FILE   "test_shared.txt"
#define RENAME_FILE "test_renamed.txt"

static int g_fid = -1;     /* file ID registered in file_rw module */

/* ── task argument structs ──────────────────────────────────── */
typedef struct {
    int thread_id;
    int fid;
    int sleep_ms;           /* simulated work time */
} read_arg_t;

typedef struct {
    int   thread_id;
    int   fid;
    char  data[256];
} write_arg_t;

/* ── thread task functions ──────────────────────────────────── */

/* Reader task: acquire rdlock, "read" for sleep_ms, release */
static void task_read(void *arg)
{
    read_arg_t *r = (read_arg_t *)arg;

    log_operation(">>> [Thread-%d] Requesting READ on fid=%d ...",
                  r->thread_id, r->fid);

    ssize_t bytes = file_rw_read(r->fid, r->thread_id);

    /* Simulate processing time INSIDE the read lock (done in file_rw_read) */
    usleep((unsigned)(r->sleep_ms) * 1000u);

    log_operation("<<< [Thread-%d] READ complete — %zd bytes", r->thread_id, bytes);
    free(r);
}

/* Writer task: acquire wrlock, write data, release */
static void task_write(void *arg)
{
    write_arg_t *w = (write_arg_t *)arg;

    log_operation(">>> [Thread-%d] Requesting WRITE on fid=%d ...",
                  w->thread_id, w->fid);

    int ret = file_rw_write(w->fid, w->thread_id, w->data, strlen(w->data));

    log_operation("<<< [Thread-%d] WRITE %s", w->thread_id,
                  ret == 0 ? "succeeded" : "FAILED");
    free(w);
}

/* ── helper: submit a reader ────────────────────────────────── */
static void submit_reader(thread_pool_t *pool, int tid, int fid, int sleep_ms)
{
    read_arg_t *a = malloc(sizeof(read_arg_t));
    a->thread_id = tid;
    a->fid       = fid;
    a->sleep_ms  = sleep_ms;
    thread_pool_submit(pool, task_read, a);
}

/* ── helper: submit a writer ────────────────────────────────── */
static void submit_writer(thread_pool_t *pool, int tid, int fid, const char *msg)
{
    write_arg_t *a = malloc(sizeof(write_arg_t));
    a->thread_id = tid;
    a->fid       = fid;
    strncpy(a->data, msg, sizeof(a->data) - 1);
    a->data[sizeof(a->data) - 1] = '\0';
    thread_pool_submit(pool, task_write, a);
}

/* ── main ───────────────────────────────────────────────────── */
int main(void)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  Multithreaded File Management System            ║\n");
    printf("║  Core Module: file_rw.c (Read/Write with rwlock) ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* ── Initialise subsystems ─────────────────────────────── */
    if (logger_init() != 0) {
        fprintf(stderr, "ERROR: logger_init failed\n");
        return 1;
    }
    log_operation("====== SYSTEM START ======");

    if (file_rw_init() != 0) {
        fprintf(stderr, "ERROR: file_rw_init failed\n");
        return 1;
    }

    /* Register the test file */
    g_fid = file_rw_register(TEST_FILE);
    if (g_fid < 0) {
        fprintf(stderr, "ERROR: file_rw_register failed\n");
        return 1;
    }
    printf("[INIT] '%s' registered as fid=%d\n\n", TEST_FILE, g_fid);

    /* Thread pool with 6 workers */
    thread_pool_t *pool = thread_pool_init(6);
    if (!pool) {
        fprintf(stderr, "ERROR: thread_pool_init failed\n");
        return 1;
    }
    log_operation("SYSTEM: Thread pool (6 workers) ready.");

    /* ════════════════════════════════════════════════════════
     * PHASE 1 — WRITE INITIAL CONTENT
     * Sequential writes to populate the file before reading.
     * ════════════════════════════════════════════════════════ */
    printf("─── PHASE 1: Writing initial content ───────────────\n");
    log_operation("====== PHASE 1: Initial writes ======");

    submit_writer(pool, 101, g_fid, "Line 1: Initializing shared file content.");
    submit_writer(pool, 102, g_fid, "Line 2: Operating Systems Lab — Team 13.");
    submit_writer(pool, 103, g_fid, "Line 3: pthread_rwlock concurrent read/write demo.");
    sleep(1);   /* wait for writes to complete before reading */

    /* ════════════════════════════════════════════════════════
     * PHASE 2 — 6 CONCURRENT READERS
     * All 6 readers fire simultaneously. Because they use
     * pthread_rwlock_rdlock they ALL enter the "critical section"
     * at the same time — demonstrating concurrent read access.
     * ════════════════════════════════════════════════════════ */
    printf("\n─── PHASE 2: 6 Concurrent Readers ──────────────────\n");
    printf("    All readers acquire rdlock simultaneously.\n");
    printf("    Watch ops.log: all 6 'READ LOCK acquired' appear\n");
    printf("    before any 'READ LOCK released'.\n\n");
    log_operation("====== PHASE 2: Concurrent reads ======");

    for (int i = 1; i <= 6; i++) {
        submit_reader(pool, i, g_fid, 200);   /* each "reads" for 200ms */
    }
    sleep(2);   /* let all readers finish */

    /* ════════════════════════════════════════════════════════
     * PHASE 3 — WRITER COMPETING WITH READERS
     * 4 readers start, then 1 writer is submitted.
     * Writer (wrlock) must wait for all 4 readers to finish.
     * Then 2 more readers start — they must wait for the writer.
     * ════════════════════════════════════════════════════════ */
    printf("\n─── PHASE 3: Writer Competing With Readers ─────────\n");
    printf("    4 readers start → writer queued → 2 late readers.\n");
    printf("    Writer waits for initial readers, then blocks late\n");
    printf("    readers until write is done. See ops.log order.\n\n");
    log_operation("====== PHASE 3: Writer vs readers ======");

    /* First wave: 4 concurrent readers (each holds lock for 300ms) */
    for (int i = 10; i <= 13; i++) {
        submit_reader(pool, i, g_fid, 300);
    }
    usleep(50000); /* tiny delay so readers get their rdlocks first */

    /* Writer: will block until all 4 readers release */
    submit_writer(pool, 50, g_fid, "EXCLUSIVE WRITE: Phase 3 writer. Readers must wait!");

    usleep(10000); /* tiny delay */

    /* Late readers: will block until writer releases wrlock */
    submit_reader(pool, 20, g_fid, 100);
    submit_reader(pool, 21, g_fid, 100);

    sleep(3);   /* let phase 3 finish */

    /* ════════════════════════════════════════════════════════
     * PHASE 4 — RENAME (exclusive write lock)
     * ════════════════════════════════════════════════════════ */
    printf("\n─── PHASE 4: Rename Under Write Lock ───────────────\n");
    log_operation("====== PHASE 4: Rename ======");

    int ren_fid = file_rw_register(TEST_FILE);
    if (file_rw_rename(ren_fid, 200, RENAME_FILE) == 0) {
        printf("    '%s' → '%s' (exclusive lock, no races)\n\n",
               TEST_FILE, RENAME_FILE);
    }
    sleep(1);

    /* ════════════════════════════════════════════════════════
     * PHASE 5 — DELETE (exclusive write lock)
     * ════════════════════════════════════════════════════════ */
    printf("\n─── PHASE 5: Delete Under Write Lock ───────────────\n");
    log_operation("====== PHASE 5: Delete ======");

    int del_fid = file_rw_register(RENAME_FILE);
    if (file_rw_delete(del_fid, 300) == 0) {
        printf("    '%s' deleted under wrlock.\n\n", RENAME_FILE);
    }

    /* ── Shutdown ───────────────────────────────────────────── */
    thread_pool_shutdown(pool);

    file_rw_destroy();
    log_operation("====== SYSTEM STOP ======");
    logger_close();

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  All phases complete. See ops.log for full trace. ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    return 0;
}

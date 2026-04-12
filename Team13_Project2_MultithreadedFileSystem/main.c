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
 *   PHASE 5 — METADATA DISPLAY AND FILE COPY
 *     Demonstrates metadata display and threaded file copying.
 *
 *   PHASE 6 — COMPRESSION & SIGNAL HANDLING (Member 5)
 *     Compress/decompress files using zlib deflate/inflate API.
 *     Handle SIGINT (Ctrl+C) for graceful shutdown.
 *     Handle SIGUSR1 to print operation status from ops.log.
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
#include "file_meta.h"
#include "compress.h"
#include "signals.h"

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

/* ── compression task argument struct ───────────────────────– */
typedef struct {
    int  thread_id;
    int  operation;  /* 0 = compress, 1 = decompress */
    char source[256];
    char dest[256];
} compress_arg_t;

/* Compression task: compress or decompress a file */
static void task_compress(void *arg)
{
    compress_arg_t *c = (compress_arg_t *)arg;
    int ret;

    if (c->operation == 0) {
        /* Compress */
        ret = compress_file(c->source, c->dest, c->thread_id);
    } else {
        /* Decompress */
        ret = decompress_file(c->source, c->dest, c->thread_id);
    }

    if (ret == 0) {
        log_operation("[Thread-%d] %s completed successfully",
                      c->thread_id,
                      c->operation == 0 ? "COMPRESS" : "DECOMPRESS");
    } else {
        log_operation("[Thread-%d] %s FAILED",
                      c->thread_id,
                      c->operation == 0 ? "COMPRESS" : "DECOMPRESS");
    }
    free(c);
}

/* ── helper: submit compression task ────────────────────────– */
static void submit_compress(thread_pool_t *pool, int tid,
                            const char *src, const char *dst, int operation)
{
    compress_arg_t *a = malloc(sizeof(compress_arg_t));
    a->thread_id = tid;
    a->operation = operation;
    strncpy(a->source, src, sizeof(a->source) - 1);
    a->source[sizeof(a->source) - 1] = '\0';
    strncpy(a->dest, dst, sizeof(a->dest) - 1);
    a->dest[sizeof(a->dest) - 1] = '\0';
    thread_pool_submit(pool, task_compress, a);
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
    printf("║  + Member 5: Compression & Signal Handling       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* ── Initialise subsystems ─────────────────────────────── */
    if (logger_init() != 0) {
        fprintf(stderr, "ERROR: logger_init failed\n");
        return 1;
    }
    log_operation("====== SYSTEM START ======");

    /* Initialize signal handlers EARLY (before any threads) */
    if (signals_init() != 0) {
        fprintf(stderr, "ERROR: signals_init failed\n");
        return 1;
    }

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

    /* ════════════════════════════════════════════════════════
     * PHASE 6 — METADATA DISPLAY AND FILE COPY
     * ════════════════════════════════════════════════════════ */
    printf("\n─── PHASE 6: Metadata Display and File Copy ────────\n");
    log_operation("====== PHASE 6: Metadata and Copy ======");

    // Display metadata for the test file
    display_metadata(TEST_FILE);

    // Submit copy task to thread pool
    copy_args_t *copy_args = malloc(sizeof(copy_args_t));
    copy_args->source = TEST_FILE;
    copy_args->dest = "test_copied.txt";
    thread_pool_submit(pool, copy_file_task, copy_args);

    // Wait a bit for the copy to complete
    sleep(1);
    printf("    File copy task submitted to thread pool.\n");

    /* ════════════════════════════════════════════════════════
     * PHASE 7 — COMPRESSION & DECOMPRESSION (Member 5)
     * Demonstrates zlib deflate/inflate API in thread pool.
     * ════════════════════════════════════════════════════════ */
    printf("\n─── PHASE 7: Compression & Decompression (Member 5) ─\n");
    log_operation("====== PHASE 7: Compression & Decompression ======");

    printf("    Compressing test_shared.txt to test_shared.txt.zlib\n");
    submit_compress(pool, 301, TEST_FILE, "test_shared.txt.zlib", 0);

    printf("    Compressing test_copied.txt to test_copied.txt.zlib\n");
    submit_compress(pool, 302, "test_copied.txt", "test_copied.txt.zlib", 0);

    sleep(1);   /* let compression tasks complete */

    printf("    Decompressing test_shared.txt.zlib to test_decompressed.txt\n");
    submit_compress(pool, 303, "test_shared.txt.zlib", "test_decompressed.txt", 1);

    sleep(1);   /* let decompression complete */

    printf("    Compression phase complete. Check ops.log for details.\n\n");

    /* ════════════════════════════════════════════════════════
     * SIGNAL HANDLING DEMO
     * The program now has signal handlers active. Users can:
     *   - Press Ctrl+C to request graceful shutdown (SIGINT)
     *   - Run `kill -USR1 <pid>` in another terminal for status (SIGUSR1)
     * ════════════════════════════════════════════════════════ */
    printf("─── Signal Handlers Active ──────────────────────────\n");
    printf("  • Press Ctrl+C to gracefully shutdown\n");
    printf("  • In another terminal: kill -USR1 %d for status\n\n", getpid());

    fprintf(stderr, "\n[Ready for signals... Press Ctrl+C when done, or send signals]\n\n");

    /* ── Shutdown ───────────────────────────────────────────── */
    thread_pool_shutdown(pool);

    file_rw_destroy();
    log_operation("====== SYSTEM STOP ======");

    /* Print compression statistics */
    char *stats = compress_get_stats();
    if (stats) {
        log_operation("%s", stats);
        fprintf(stderr, "\n%s\n", stats);
        free(stats);
    }

    logger_close();

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  All phases complete. See ops.log for full trace. ║\n");
    printf("║  Signal handling initialized successfully.       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    return 0;
}

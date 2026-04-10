/*
 * file_rw.h — Core Read/Write Module for Multithreaded File System
 *
 * This module is the HEART of the project.
 *
 * Design:
 *   Every logical "file" tracked by this system has its own
 *   pthread_rwlock_t.  Multiple threads can hold READ locks on the
 *   same file simultaneously (concurrent reads).  A WRITE lock is
 *   exclusive — no reader or other writer may run concurrently.
 *
 *   The file table is a fixed-size array protected by a global mutex.
 *   All operations are logged via logger.h.
 *
 * Synchronization summary
 * ─────────────────────────────────────────────────────────────
 *  Operation        Lock acquired          Allowed concurrency
 *  ──────────────   ─────────────────────  ────────────────────
 *  file_rw_read()   pthread_rwlock_rdlock  Many readers at once
 *  file_rw_write()  pthread_rwlock_wrlock  ONE writer, zero readers
 *  file_rw_delete() pthread_rwlock_wrlock  ONE writer, zero readers
 *  file_rw_rename() pthread_rwlock_wrlock  ONE writer, zero readers
 * ─────────────────────────────────────────────────────────────
 */

#ifndef FILE_RW_H
#define FILE_RW_H

#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>

/* ── tunables ──────────────────────────────────────────────── */
#define FILE_TABLE_SIZE  32     /* max open/tracked files        */
#define MAX_FILENAME_LEN 256    /* maximum path length           */
#define READ_BUF_SIZE    4096   /* bytes read per call           */

/* ── per-file entry ────────────────────────────────────────── */
typedef struct {
    char            path[MAX_FILENAME_LEN]; /* absolute or relative path  */
    pthread_rwlock_t rwlock;                /* per-file read/write lock   */
    int             in_use;                 /* 1 = registered, 0 = free   */
    int             active_readers;         /* informational counter       */
} file_entry_t;

/* ── module API ────────────────────────────────────────────── */

/**
 * file_rw_init()
 *   Must be called once before any other function in this module.
 *   Initialises the file table and its protective mutex.
 *   Returns 0 on success, -1 on error.
 */
int  file_rw_init(void);

/**
 * file_rw_register(path)
 *   Register a file path in the table and create its rwlock.
 *   Returns a non-negative file-ID (fid) on success, -1 on error.
 *   If the file is already registered the existing fid is returned.
 */
int  file_rw_register(const char *path);

/**
 * file_rw_read(fid, thread_id)
 *   Multiple threads may call this concurrently for the SAME fid.
 *   Acquires pthread_rwlock_rdlock → reads the whole file → releases.
 *   Returns number of bytes read on success, -1 on error.
 */
ssize_t file_rw_read(int fid, int thread_id);

/**
 * file_rw_write(fid, thread_id, data, len)
 *   Exclusive write: acquires pthread_rwlock_wrlock, appends/writes
 *   `data` (len bytes) to the file, releases.
 *   Returns 0 on success, -1 on error.
 */
int  file_rw_write(int fid, int thread_id, const char *data, size_t len);

/**
 * file_rw_delete(fid, thread_id)
 *   Write-locks the entry, removes the underlying file from disk,
 *   then unregisters the fid.
 *   Returns 0 on success, -1 on error.
 */
int  file_rw_delete(int fid, int thread_id);

/**
 * file_rw_rename(fid, thread_id, new_path)
 *   Write-locks the entry, renames the file on disk, updates the
 *   internal path record.
 *   Returns 0 on success, -1 on error.
 */
int  file_rw_rename(int fid, int thread_id, const char *new_path);

/**
 * file_rw_destroy()
 *   Destroy all rwlocks and clean up. Call after all threads finish.
 */
void file_rw_destroy(void);

#endif /* FILE_RW_H */

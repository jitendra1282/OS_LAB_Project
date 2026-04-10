#ifndef RWLOCK_MANAGER_H
#define RWLOCK_MANAGER_H

#include <pthread.h>

/**
 * Global Read-Write Lock for Filesystem operations.
 */
extern pthread_rwlock_t fs_rwlock;

/**
 * Initialize the global rwlock.
 */
int fs_rwlock_init(void);

/**
 * Wrappers for read and write locking.
 */
void fs_read_lock(void);
void fs_write_lock(void);
void fs_unlock(void);

/**
 * Destroy the global rwlock.
 */
void fs_rwlock_destroy(void);

#endif // RWLOCK_MANAGER_H

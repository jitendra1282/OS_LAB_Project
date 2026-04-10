#include "rwlock_manager.h"

pthread_rwlock_t fs_rwlock;

int fs_rwlock_init(void) {
    return pthread_rwlock_init(&fs_rwlock, NULL);
}

void fs_read_lock(void) {
    pthread_rwlock_rdlock(&fs_rwlock);
}

void fs_write_lock(void) {
    pthread_rwlock_wrlock(&fs_rwlock);
}

void fs_unlock(void) {
    pthread_rwlock_unlock(&fs_rwlock);
}

void fs_rwlock_destroy(void) {
    pthread_rwlock_destroy(&fs_rwlock);
}

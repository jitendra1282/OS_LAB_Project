#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_pool.h"
#include "logger.h"
#include "rwlock_manager.h"

void demo_task(void *arg) {
    int task_id = *(int *)arg;
    free(arg); // Free dynamically allocated argument

    log_operation("Task %d: Started", task_id);

    // Simulate reading
    fs_read_lock();
    log_operation("Task %d: Acquired READ lock. Reading fake filesystem data.", task_id);
    usleep(100000); // Sleep for 100ms
    fs_unlock();
    log_operation("Task %d: Released READ lock.", task_id);

    // Simulate writing occasionally
    if (task_id % 3 == 0) {
        fs_write_lock();
        log_operation("Task %d: Acquired WRITE lock. Modifying fake filesystem data.", task_id);
        usleep(200000); // Sleep for 200ms
        fs_unlock();
        log_operation("Task %d: Released WRITE lock.", task_id);
    }

    log_operation("Task %d: Completed", task_id);
}

int main(void) {
    printf("Starting Core Foundation Initialization...\n");

    // Initialize Logger
    if (logger_init() != 0) {
        fprintf(stderr, "Failed to initialize logger.\n");
        return 1;
    }
    log_operation("SYSTEM: Logger initialized.");

    // Initialize RWLock
    if (fs_rwlock_init() != 0) {
        log_operation("SYSTEM: Failed to initialize RWLock.");
        return 1;
    }
    log_operation("SYSTEM: RWLock initialized.");

    // Initialize Thread Pool with 4 threads
    thread_pool_t *pool = thread_pool_init(4);
    if (!pool) {
        log_operation("SYSTEM: Failed to initialize Thread Pool.");
        return 1;
    }
    log_operation("SYSTEM: Thread pool of size 4 initialized.");

    // Submit 10 tasks to the thread pool
    for (int i = 1; i <= 10; i++) {
        int *arg = malloc(sizeof(int));
        *arg = i;
        thread_pool_submit(pool, demo_task, arg);
    }

    log_operation("SYSTEM: Submitted all tasks. Waiting for completion...");

    // Shutdown pool (this blocks until all submitted tasks map finish execution)
    thread_pool_shutdown(pool);
    log_operation("SYSTEM: Thread pool shutdown cleanly.");

    // Destroy RWLock
    fs_rwlock_destroy();
    log_operation("SYSTEM: RWLock destroyed.");

    // Close logger
    log_operation("SYSTEM: Process exiting cleanly.");
    logger_close();

    printf("Check ops.log for the execution trace!\n");

    return 0;
}

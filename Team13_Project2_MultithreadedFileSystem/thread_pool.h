#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

/**
 * Task structure for the thread pool
 */
typedef struct task {
    void (*function)(void *);
    void *argument;
    struct task *next;
} task_t;

/**
 * Thread Pool structure
 */
typedef struct thread_pool {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    task_t *task_queue_head;
    task_t *task_queue_tail;
    int thread_count;
    int queue_size;
    bool shutdown;
} thread_pool_t;

/**
 * Initializes a thread pool.
 * @param num_threads The number of worker threads to spawn.
 * @return A pointer to the initialized thread pool, or NULL on failure.
 */
thread_pool_t *thread_pool_init(int num_threads);

/**
 * Submits a new task to the thread pool queue.
 * @param pool The thread pool.
 * @param function The function to execute.
 * @param argument The argument to pass to the function.
 * @return 0 on success, -1 on failure.
 */
int thread_pool_submit(thread_pool_t *pool, void (*function)(void *), void *argument);

/**
 * Shuts down the thread pool cleanly.
 * Waits for all threads to finish their current tasks.
 * @param pool The thread pool to shutdown.
 */
void thread_pool_shutdown(thread_pool_t *pool);

#endif // THREAD_POOL_H

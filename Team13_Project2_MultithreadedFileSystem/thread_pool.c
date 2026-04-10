#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * The worker thread loop.
 */
static void *thread_worker(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->lock);
        
        // Wait until there is a task or the pool is shutting down
        while (pool->queue_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }
        
        // If shutting down and no more tasks, exit the thread
        if (pool->shutdown && pool->queue_size == 0) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }
        
        // Pop task from queue
        task_t *task = pool->task_queue_head;
        if (task != NULL) {
            pool->task_queue_head = task->next;
            if (pool->task_queue_head == NULL) {
                pool->task_queue_tail = NULL;
            }
            pool->queue_size--;
        }
        
        pthread_mutex_unlock(&pool->lock);
        
        // Execute the task
        if (task != NULL) {
            (*(task->function))(task->argument);
            free(task);
        }
    }
    
    return NULL;
}

thread_pool_t *thread_pool_init(int num_threads) {
    if (num_threads <= 0) return NULL;
    
    thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (pool == NULL) return NULL;
    
    pool->thread_count = num_threads;
    pool->queue_size = 0;
    pool->task_queue_head = NULL;
    pool->task_queue_tail = NULL;
    pool->shutdown = false;
    
    if (pthread_mutex_init(&pool->lock, NULL) != 0 ||
        pthread_cond_init(&pool->notify, NULL) != 0) {
        free(pool);
        return NULL;
    }
    
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    if (pool->threads == NULL) {
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
        free(pool);
        return NULL;
    }
    
    // Spawn threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, thread_worker, (void *)pool) != 0) {
            thread_pool_shutdown(pool);
            return NULL;
        }
    }
    
    return pool;
}

int thread_pool_submit(thread_pool_t *pool, void (*function)(void *), void *argument) {
    if (pool == NULL || function == NULL) return -1;
    
    task_t *new_task = (task_t *)malloc(sizeof(task_t));
    if (new_task == NULL) return -1;
    
    new_task->function = function;
    new_task->argument = argument;
    new_task->next = NULL;
    
    pthread_mutex_lock(&pool->lock);
    
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        free(new_task);
        return -1;
    }
    
    // Add task to queue
    if (pool->task_queue_tail == NULL) {
        pool->task_queue_head = new_task;
        pool->task_queue_tail = new_task;
    } else {
        pool->task_queue_tail->next = new_task;
        pool->task_queue_tail = new_task;
    }
    
    pool->queue_size++;
    
    // Wake up one worker thread
    pthread_cond_signal(&pool->notify);
    pthread_mutex_unlock(&pool->lock);
    
    return 0;
}

void thread_pool_shutdown(thread_pool_t *pool) {
    if (pool == NULL) return;
    
    pthread_mutex_lock(&pool->lock);
    if (pool->shutdown) { /* Already shutting down */
        pthread_mutex_unlock(&pool->lock);
        return;
    }
    
    pool->shutdown = true;
    
    // Wake up all threads so they can exit
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);
    
    // Wait for all threads to terminate
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // Free resources
    free(pool->threads);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify);
    
    // Free any unexecuted tasks
    task_t *curr = pool->task_queue_head;
    while (curr != NULL) {
        task_t *next = curr->next;
        free(curr);
        curr = next;
    }
    
    free(pool);
}

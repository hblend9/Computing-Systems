#include "thread_pool.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <pthread.h>

typedef struct tpool_work {
    work_function_t func;
    void *arg;
    struct tpool_work *next;
} tpool_work_t;

typedef struct thread_pool { 
    queue_t *queue;
    pthread_t *threads;
    size_t num_threads;
} thread_pool_t;

void *work(void *aux) {
    tpool_work_t *value = queue_dequeue((queue_t *) aux);
    while (value != NULL) {
        value->func(value->arg);
        free(value);
        value = queue_dequeue((queue_t *) aux);
    }
    return NULL;
}

thread_pool_t *thread_pool_init(size_t num_worker_threads) {
    thread_pool_t *pool = malloc(sizeof(tpool_work_t));
    pool->queue = queue_init();
    pool->threads = malloc(sizeof(pthread_t) * num_worker_threads);
    pool->num_threads = num_worker_threads;
    for (size_t i = 0; i < num_worker_threads; i++) {
        pthread_create(&pool->threads[i], NULL, work, pool->queue);
    }
    return pool;
}

void thread_pool_add_work(thread_pool_t *pool, work_function_t function, void *aux) {
    tpool_work_t *work = malloc(sizeof(tpool_work_t));
    work->func = function;
    work->arg = aux;
    queue_enqueue(pool->queue, work);
}

void thread_pool_finish(thread_pool_t *pool) {
    for (size_t i = 0; i < pool->num_threads; i++) {
        queue_enqueue(pool->queue, NULL); // once it reaches NULL, it will do the work
    }
    for (size_t i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL); // join after you get the NULL
    }
    queue_free(pool->queue);
    free(pool->threads);
    free(pool);
}

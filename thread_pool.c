#include "thread_pool.h"

#include <stdlib.h>

typedef struct {
    threadpool_t *pool;
    int worker_id;
} worker_arg_t;

static void *worker_loop(void *arg) {
    worker_arg_t *warg = arg;
    threadpool_t *pool = warg->pool;
    int worker_id = warg->worker_id;

    /*
     * MCS locks require each thread to have its own node.
     * This node lives for the entire lifetime of the worker thread.
     */
    mcs_node_t mcs_node;
    mylock_set_mcs_node(&mcs_node);

    free(warg);

    for (;;) {
        task_t task = jobqueue_pop(&pool->queue);

        /*
         * A NULL function is used as a shutdown marker.
         * Each worker exits after receiving one shutdown task.
         */
        if (task.fn == NULL) {
            break;
        }

        task.fn(task.arg);

        /* Used by benchmarks to measure how evenly work is distributed. */
        pool->tasks_done[worker_id]++;
    }

    return NULL;
}

void threadpool_init_kind(threadpool_t *pool,
                          int num_threads,
                          int queue_capacity,
                          mylock_kind_t kind) {
    pool->num_threads = num_threads;
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->tasks_done = calloc(num_threads, sizeof(long));

    jobqueue_init_kind(&pool->queue, queue_capacity, kind);

    for (int i = 0; i < num_threads; i++) {
        worker_arg_t *arg = malloc(sizeof(worker_arg_t));
        arg->pool = pool;
        arg->worker_id = i;

        pthread_create(&pool->threads[i], NULL, worker_loop, arg);
    }
}

void threadpool_init(threadpool_t *pool,
                     int num_threads,
                     int queue_capacity) {
    threadpool_init_kind(pool, num_threads, queue_capacity, MYLOCK_FUTEX);
}

void threadpool_submit(threadpool_t *pool, task_fn_t fn, void *arg) {
    jobqueue_push(&pool->queue, fn, arg);
}

void threadpool_destroy(threadpool_t *pool) {
    /*
     * Submit one shutdown marker per worker so every worker eventually
     * wakes, dequeues a marker, and exits its loop.
     */
    for (int i = 0; i < pool->num_threads; i++) {
        threadpool_submit(pool, NULL, NULL);
    }

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    jobqueue_destroy(&pool->queue);

    free(pool->threads);
    pool->threads = NULL;

    /*
     * tasks_done is intentionally not freed here because benchmarks read it
     * after joining workers. The benchmark frees it after collecting metrics.
     */
}
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include "job_queue.h"

typedef struct {
    pthread_t *threads;
    int num_threads;
    long *tasks_done;
    jobqueue_t queue;
} threadpool_t;

void threadpool_init(threadpool_t *pool, int num_threads, int queue_capacity);
void threadpool_init_kind(threadpool_t *pool, int num_threads, int queue_capacity, mylock_kind_t kind);
void threadpool_submit(threadpool_t *pool, task_fn_t fn, void *arg);
void threadpool_destroy(threadpool_t *pool);

#endif
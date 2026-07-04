#include "job_queue.h"

#include <stdlib.h>

void jobqueue_init_kind(jobqueue_t *q, int capacity, mylock_kind_t kind) {
    q->tasks = malloc(sizeof(task_t) * capacity);

    q->capacity = capacity;
    q->count = 0;
    q->head = 0;
    q->tail = 0;

    mylock_init_kind(&q->lock, kind);
    mycond_init(&q->not_empty);
    mycond_init(&q->not_full);
}

void jobqueue_init(jobqueue_t *q, int capacity) {
    jobqueue_init_kind(q, capacity, MYLOCK_FUTEX);
}

void jobqueue_destroy(jobqueue_t *q) {
    free(q->tasks);
    q->tasks = NULL;
}

void jobqueue_push(jobqueue_t *q, task_fn_t fn, void *arg) {
    mylock_acquire(&q->lock);

    while (q->count == q->capacity) {
        mycond_wait(&q->not_full, &q->lock);
    }

    q->tasks[q->tail].fn = fn;
    q->tasks[q->tail].arg = arg;

    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    mycond_signal(&q->not_empty);

    mylock_release(&q->lock);
}

task_t jobqueue_pop(jobqueue_t *q) {
    mylock_acquire(&q->lock);

    while (q->count == 0) {
        mycond_wait(&q->not_empty, &q->lock);
    }

    task_t task = q->tasks[q->head];

    q->head = (q->head + 1) % q->capacity;
    q->count--;

    mycond_signal(&q->not_full);

    mylock_release(&q->lock);

    return task;
}
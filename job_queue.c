#include "job_queue.h"

#include <stdlib.h>

void jobqueue_init_kind(jobqueue_t *q, int capacity, mylock_kind_t kind) {
    q->tasks = malloc(sizeof(task_t) * capacity);

    q->capacity = capacity;
    q->count = 0;
    q->head = 0;
    q->tail = 0;

    /*
     * The queue lock protects head, tail, count, and the task buffer.
     * The lock kind is configurable so the same queue can benchmark
     * different lock implementations.
     */
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

    /*
     * The queue is bounded. Producers wait while the buffer is full.
     * The while loop handles spurious wakeups or wakeups where another
     * producer filled the queue first.
     */
    while (q->count == q->capacity) {
        mycond_wait(&q->not_full, &q->lock);
    }

    /* Insert at tail, then wrap around using circular-buffer indexing. */
    q->tasks[q->tail].fn = fn;
    q->tasks[q->tail].arg = arg;

    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    /* Wake one worker that may be blocked waiting for work. */
    mycond_signal(&q->not_empty);

    mylock_release(&q->lock);
}

task_t jobqueue_pop(jobqueue_t *q) {
    mylock_acquire(&q->lock);

    /*
     * Consumers wait while the queue is empty.
     * The condition is rechecked after waking.
     */
    while (q->count == 0) {
        mycond_wait(&q->not_empty, &q->lock);
    }

    /* Remove from head, then wrap around using circular-buffer indexing. */
    task_t task = q->tasks[q->head];

    q->head = (q->head + 1) % q->capacity;
    q->count--;

    /* Wake one producer that may be blocked because the queue was full. */
    mycond_signal(&q->not_full);

    mylock_release(&q->lock);

    return task;
}
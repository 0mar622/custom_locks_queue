#ifndef JOBQUEUE_H
#define JOBQUEUE_H

#include "mylock.h"

typedef void (*task_fn_t)(void *);

typedef struct {
    task_fn_t fn;
    void *arg;
} task_t;

typedef struct {
    task_t *tasks;
    int capacity;
    int count;
    int head;
    int tail;

    mylock_t lock;
    mycond_t not_empty;
    mycond_t not_full;
} jobqueue_t;

void jobqueue_init(jobqueue_t *q, int capacity);
void jobqueue_init_kind(jobqueue_t *q, int capacity, mylock_kind_t kind);
void jobqueue_destroy(jobqueue_t *q);

void jobqueue_push(jobqueue_t *q, task_fn_t fn, void *arg);
task_t jobqueue_pop(jobqueue_t *q);

#endif
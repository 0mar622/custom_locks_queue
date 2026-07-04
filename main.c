#include <stdio.h>
#include <pthread.h>
#include "thread_pool.h"

void print_task(void *arg) {
    printf("worker %lu ran task: %s\n",
           (unsigned long)pthread_self(),
           (char *)arg);
}

int main(void) {
    threadpool_t pool;

    threadpool_init(&pool, 4, 8);

    threadpool_submit(&pool, print_task, "A");
    threadpool_submit(&pool, print_task, "B");
    threadpool_submit(&pool, print_task, "C");
    threadpool_submit(&pool, print_task, "D");
    threadpool_submit(&pool, print_task, "E");
    threadpool_submit(&pool, print_task, "F");
    threadpool_submit(&pool, print_task, "G");
    threadpool_submit(&pool, print_task, "H");

    threadpool_destroy(&pool);

    return 0;
}
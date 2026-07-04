#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

#include "mylock.h"

#define ITERS 100000

typedef enum {
    BENCH_MYLOCK,
    BENCH_PTHREAD
} bench_type_t;

typedef struct {
    int thread_id;
    long completed;
    mcs_node_t mcs_node;
} thread_arg_t;

mylock_t my_lock;
pthread_mutex_t pthread_lock;

long counter = 0;
bench_type_t current_bench;

double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void lock_acquire(void) {
    if (current_bench == BENCH_MYLOCK) {
        mylock_acquire(&my_lock);
    } else {
        pthread_mutex_lock(&pthread_lock);
    }
}

void lock_release(void) {
    if (current_bench == BENCH_MYLOCK) {
        mylock_release(&my_lock);
    } else {
        pthread_mutex_unlock(&pthread_lock);
    }
}

void *worker(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    /*
     * Each benchmark thread owns an MCS node. This call is harmless for
     * non-MCS locks and required when the selected lock is MYLOCK_MCS.
     */
    mylock_set_mcs_node(&targ->mcs_node);

    for (int i = 0; i < ITERS; i++) {
        /*
         * The critical section is intentionally tiny so the benchmark
         * stresses lock acquisition and release overhead.
         */
        lock_acquire();
        counter++;
        lock_release();

        targ->completed++;
    }

    return NULL;
}

void run_benchmark(const char *name,
                   bench_type_t type,
                   mylock_kind_t kind,
                   int num_threads) {
    pthread_t threads[num_threads];
    thread_arg_t args[num_threads];

    counter = 0;
    current_bench = type;

    /*
     * The same benchmark body is used for both the custom locks and
     * pthread_mutex_t so their behavior can be compared directly.
     */
    if (type == BENCH_MYLOCK) {
        mylock_init_kind(&my_lock, kind);
    } else {
        pthread_mutex_init(&pthread_lock, NULL);
    }

    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].completed = 0;
    }

    double start = now_sec();

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    double end = now_sec();

    if (type == BENCH_PTHREAD) {
        pthread_mutex_destroy(&pthread_lock);
    }

    long expected = (long)num_threads * ITERS;
    double elapsed = end - start;
    double throughput = expected / elapsed;

    /*
     * Each thread performs the same fixed number of iterations. These
     * fairness fields mainly verify that every thread completed its work.
     */
    long min = args[0].completed;
    long max = args[0].completed;
    double sum = 0.0;

    for (int i = 0; i < num_threads; i++) {
        if (args[i].completed < min) min = args[i].completed;
        if (args[i].completed > max) max = args[i].completed;
        sum += args[i].completed;
    }

    double mean = sum / num_threads;
    double variance = 0.0;

    for (int i = 0; i < num_threads; i++) {
        double diff = args[i].completed - mean;
        variance += diff * diff;
    }

    variance /= num_threads;

    printf("%-10s | %2d threads | time: %.4f s | throughput: %.0f ops/s | counter: %ld/%ld | fairness variance: %.2f | min: %ld | max: %ld\n",
           name,
           num_threads,
           elapsed,
           throughput,
           counter,
           expected,
           variance,
           min,
           max);
}

int main(void) {
    int thread_counts[] = {1, 2, 4, 8};
    int n = sizeof(thread_counts) / sizeof(thread_counts[0]);

    for (int i = 0; i < n; i++) {
        int t = thread_counts[i];

        run_benchmark("spin", BENCH_MYLOCK, MYLOCK_SPIN, t);
        run_benchmark("ticket", BENCH_MYLOCK, MYLOCK_TICKET, t);
        run_benchmark("futex", BENCH_MYLOCK, MYLOCK_FUTEX, t);
        run_benchmark("pthread", BENCH_PTHREAD, MYLOCK_SPIN, t);
        run_benchmark("mcs", BENCH_MYLOCK, MYLOCK_MCS, t);

        printf("\n");
    }
}
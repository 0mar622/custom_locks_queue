#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#include "thread_pool.h"

#define NUM_TASKS 100000

/* Counts total completed tasks across all workers. */
atomic_int completed = 0;

double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void benchmark_task(void *arg) {
    (void)arg;

    /*
     * Small artificial workload so the benchmark measures both queue
     * synchronization and useful work outside the critical section.
     */
    volatile long x = 0;
    for (int i = 0; i < 1000; i++) {
        x += i;
    }

    atomic_fetch_add_explicit(&completed, 1, memory_order_relaxed);
}

const char *lock_name(mylock_kind_t kind) {
    if (kind == MYLOCK_SPIN) return "spin";
    if (kind == MYLOCK_TICKET) return "ticket";
    if (kind == MYLOCK_FUTEX) return "futex";
    if (kind == MYLOCK_MCS) return "mcs";
    return "unknown";
}

void run_benchmark(mylock_kind_t kind, int num_workers) {
    threadpool_t pool;

    atomic_store(&completed, 0);

    /*
     * The main thread submits jobs into the queue, so it may acquire the
     * queue lock. If the queue uses MCS, the main thread needs a node too.
     */
    mcs_node_t main_mcs_node;
    mylock_set_mcs_node(&main_mcs_node);

    double start = now_sec();

    threadpool_init_kind(&pool, num_workers, 128, kind);

    for (int i = 0; i < NUM_TASKS; i++) {
        threadpool_submit(&pool, benchmark_task, NULL);
    }

    /*
     * Joining happens inside threadpool_destroy. tasks_done remains
     * allocated so this benchmark can compute fairness metrics.
     */
    threadpool_destroy(&pool);

    long min = pool.tasks_done[0];
    long max = pool.tasks_done[0];

    for (int i = 0; i < num_workers; i++) {
        if (pool.tasks_done[i] < min) min = pool.tasks_done[i];
        if (pool.tasks_done[i] > max) max = pool.tasks_done[i];
    }

    /*
     * Fairness is measured by comparing each worker's task count to the
     * ideal average number of tasks per worker.
     */
    double mean = (double)NUM_TASKS / num_workers;
    double variance = 0.0;

    for (int i = 0; i < num_workers; i++) {
        double diff = pool.tasks_done[i] - mean;
        variance += diff * diff;
    }

    variance /= num_workers;

    double end = now_sec();

    double elapsed = end - start;
    double throughput = NUM_TASKS / elapsed;

    printf("%-8s | %2d workers | time: %.4f s | throughput: %.0f jobs/s | completed: %d/%d | min: %ld | max: %ld | variance: %.2f\n",
           lock_name(kind),
           num_workers,
           elapsed,
           throughput,
           atomic_load(&completed),
           NUM_TASKS,
           min,
           max,
           variance);

    free(pool.tasks_done);
}

int main(void) {
    int worker_counts[] = {1, 2, 4, 8};
    int n = sizeof(worker_counts) / sizeof(worker_counts[0]);

    printf("Job Queue / Thread Pool Benchmark\n");
    printf("NUM_TASKS: %d\n\n", NUM_TASKS);

    for (int i = 0; i < n; i++) {
        int workers = worker_counts[i];

        run_benchmark(MYLOCK_SPIN, workers);
        run_benchmark(MYLOCK_TICKET, workers);
        run_benchmark(MYLOCK_FUTEX, workers);
        run_benchmark(MYLOCK_MCS, workers);

        printf("\n");
    }

    return 0;
}
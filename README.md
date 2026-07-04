# Custom Locks and Concurrent Job Queue

A C concurrency project that implements custom lock primitives, condition-variable style waiting, a bounded job queue, and a thread pool. The project compares lock behavior using both a lock microbenchmark and an end-to-end job queue benchmark.

## Features

- Spinlock using C11 atomics
- Ticket lock for FIFO-style lock acquisition
- Futex-backed mutex using Linux `futex`
- MCS queue lock
- Condition-variable style wait/signal/broadcast
- Bounded producer-consumer job queue
- Thread pool with worker threads
- Lock microbenchmark
- Job queue/thread pool benchmark
- Throughput and fairness measurements

## Project Structure

- `mylock.h`, `mylock.c` — custom locks and condition variables
- `job_queue.h`, `job_queue.c` — bounded concurrent job queue
- `thread_pool.h`, `thread_pool.c` — thread pool implementation
- `main.c` — basic thread pool demo
- `bench_locks.c` — lock microbenchmark
- `bench_queue.c` — job queue/thread pool benchmark
- `Makefile` — build rules

## Building

```bash
make
```

This builds:

- `test`
- `bench_locks`
- `bench_queue`

## Running

Run the basic thread pool demo:

```bash
./test
```

Run the lock microbenchmark:

```bash
./bench_locks
```

Run the job queue/thread pool benchmark:

```bash
./bench_queue
```

Clean build files:

```bash
make clean
```

## Benchmark Metrics

### Time

Total wall-clock time for the benchmark run.

Lower is better.

### Throughput

Amount of work completed per second.

- `bench_locks`: operations per second
- `bench_queue`: jobs per second

Higher is better.

### Correctness Count

Checks whether all expected operations completed.

Examples:

```text
counter: 800000/800000
completed: 100000/100000
```

### Min / Max

Minimum and maximum number of tasks completed by any worker thread.

Closer values indicate better fairness.

### Fairness Variance

Measures how evenly work was distributed across threads or workers.

Lower variance means better fairness.

## Benchmark Results and Observations

Benchmarks were run using 1, 2, 4, and 8 threads or workers.

### Lock Microbenchmark

`bench_locks.c` measures the locks in isolation by repeatedly incrementing a shared counter inside a critical section.

At 8 threads, the measured results were:

| Lock | Time | Throughput |
| --- | ---: | ---: |
| Spin | 0.1747 s | 4,578,052 ops/s |
| Ticket | 0.1786 s | 4,478,802 ops/s |
| Futex | 0.1172 s | 6,826,773 ops/s |
| Pthread mutex | 0.0595 s | 13,451,750 ops/s |
| MCS | 0.2083 s | 3,841,314 ops/s |

The pthread mutex was the fastest baseline in this benchmark. Among the custom locks, the futex-backed lock performed best at 8 threads.

The spinlock performed well at low thread counts because it has very little acquisition overhead. As thread count increased, performance decreased because waiting threads continuously spin and consume CPU resources.

The ticket lock provides ordered lock acquisition, but all waiting threads repeatedly check the shared `now_serving` variable, which can become a scalability bottleneck.

The futex-backed lock avoids continuous spinning by allowing waiting threads to sleep in the kernel, which reduced wasted CPU time under contention.

The MCS lock uses per-thread queue nodes so each waiting thread spins on its own node instead of repeatedly contending on one shared lock word. In this implementation, `sched_yield()` was added to the wait loops to reduce excessive CPU consumption during contention.

### Job Queue / Thread Pool Benchmark

`bench_queue.c` measures the full bounded job queue and thread pool. Tasks perform a small computational workload outside the queue critical section.

At 8 workers, the measured results were:

| Lock | Time | Throughput | Min | Max | Variance |
| --- | ---: | ---: | ---: | ---: | ---: |
| Spin | 0.2594 s | 385,492 jobs/s | 9,981 | 24,310 | 20,568,094.00 |
| Ticket | 0.2035 s | 491,350 jobs/s | 12,311 | 12,781 | 23,717.00 |
| Futex | 0.1755 s | 569,839 jobs/s | 12,352 | 12,633 | 8,661.25 |
| MCS | 0.2029 s | 492,912 jobs/s | 12,349 | 12,713 | 10,399.25 |

The queue benchmark showed that throughput improved from 1 to 4 workers because tasks could execute in parallel outside the queue critical section.

At 8 workers, synchronization overhead became more significant. The spinlock had the worst throughput and much worse fairness. One worker completed 24,310 tasks while another completed only 9,981 tasks.

Ticket, futex, and MCS locks distributed work much more evenly across worker threads. In this run, the futex-backed lock achieved the best throughput and lowest fairness variance at 8 workers.

## Key Takeaways

- More threads do not always improve performance.
- Lock performance depends heavily on workload and contention level.
- Spinlocks can be fast at low contention but can waste CPU and become unfair under higher contention.
- Ticket locks provide ordered acquisition but still involve shared-variable spinning.
- Futex-backed locks reduce active spinning by sleeping waiting threads.
- MCS locks reduce shared cache-line contention by using per-thread queue nodes.
- The end-to-end queue benchmark is more realistic than the lock-only microbenchmark because it measures the lock inside a complete producer-consumer application.

## Memory Ordering Choices

This project uses C11 `<stdatomic.h>` for atomic operations and synchronization.

### Acquire Ordering

Lock acquisition uses `memory_order_acquire`.

Acquire ordering prevents memory operations inside the critical section from being reordered before successful lock acquisition. It also allows a thread acquiring a lock to observe writes published by the previous lock owner.

### Release Ordering

Lock release uses `memory_order_release`.

Release ordering ensures that writes performed inside the critical section are completed before the lock becomes available to another thread.

Together, acquire and release ordering establish synchronization between consecutive lock owners.

### Relaxed Ordering

`memory_order_relaxed` is used when atomicity is required but additional synchronization ordering is unnecessary.

For example, ticket assignment uses relaxed ordering because the ticket value determines queue position, while ownership synchronization is handled through acquire and release operations on `now_serving`.

### Acquire-Release Ordering

The MCS lock uses `memory_order_acq_rel` when atomically exchanging the queue tail.

The release portion publishes the current thread's MCS node, while the acquire portion safely observes the previous queue tail.

### Sequential Consistency

The project avoids `memory_order_seq_cst` where stronger global ordering is unnecessary.

Acquire and release semantics provide the ordering required for lock synchronization while allowing more implementation and hardware optimization.

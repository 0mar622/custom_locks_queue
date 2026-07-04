# Custom Locks and Concurrent Job Queue

A concurrency project written in C that implements custom synchronization primitives and evaluates their behavior in a bounded job queue and thread pool.

## Features

- Spinlock
- Ticket lock
- Futex-based lock
- MCS queue lock
- Bounded concurrent job queue
- Thread pool
- Condition-variable style waiting
- Lock microbenchmarks
- End-to-end job queue benchmarks
- Throughput and fairness measurements

## Benchmarks

The project contains two benchmark programs.

### Lock Microbenchmark

`bench_locks.c` measures the synchronization primitives in isolation under different thread counts.

Metrics include:

- Execution time
- Throughput in operations per second
- Counter correctness
- Minimum and maximum work completed per thread
- Fairness variance

### Job Queue / Thread Pool Benchmark

`bench_queue.c` measures lock behavior in the full concurrent job queue and thread pool.

Worker counts of 1, 2, 4, and 8 are tested.

Metrics include:

- Execution time
- Throughput in jobs per second
- Completed task count
- Minimum tasks completed by a worker
- Maximum tasks completed by a worker
- Fairness variance

The benchmark uses a small computational workload inside each task so workers perform useful work outside the queue critical section.

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

`memory_order_relaxed` is used when atomicity is required but additional memory synchronization is unnecessary.

For example, ticket assignment can use relaxed ordering because the ticket value determines queue position, while lock ownership synchronization is handled through acquire and release operations.

### Acquire-Release Ordering

The MCS lock uses `memory_order_acq_rel` when atomically exchanging the queue tail.

The release portion publishes the current thread's MCS node, while the acquire portion safely observes the previous queue tail.

### Sequential Consistency

The project avoids `memory_order_seq_cst` where stronger global ordering is unnecessary.

Acquire and release semantics provide the ordering required for lock synchronization while allowing more implementation and hardware optimization.

## Building

```bash
make

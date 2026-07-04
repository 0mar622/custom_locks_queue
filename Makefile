CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread

TARGET = test
BENCH_LOCKS = bench_locks
BENCH_QUEUE = bench_queue

OBJS = main.o mylock.o job_queue.o thread_pool.o
BENCH_LOCKS_OBJS = bench_locks.o mylock.o
BENCH_QUEUE_OBJS = bench_queue.o mylock.o job_queue.o thread_pool.o

all: $(TARGET) $(BENCH_LOCKS) $(BENCH_QUEUE)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

$(BENCH_LOCKS): $(BENCH_LOCKS_OBJS)
	$(CC) $(CFLAGS) $(BENCH_LOCKS_OBJS) -o $(BENCH_LOCKS)

$(BENCH_QUEUE): $(BENCH_QUEUE_OBJS)
	$(CC) $(CFLAGS) $(BENCH_QUEUE_OBJS) -o $(BENCH_QUEUE)

main.o: main.c thread_pool.h
	$(CC) $(CFLAGS) -c main.c

bench_locks.o: bench_locks.c mylock.h
	$(CC) $(CFLAGS) -c bench_locks.c

bench_queue.o: bench_queue.c thread_pool.h job_queue.h mylock.h
	$(CC) $(CFLAGS) -c bench_queue.c

mylock.o: mylock.c mylock.h
	$(CC) $(CFLAGS) -c mylock.c

job_queue.o: job_queue.c job_queue.h mylock.h
	$(CC) $(CFLAGS) -c job_queue.c

thread_pool.o: thread_pool.c thread_pool.h job_queue.h mylock.h
	$(CC) $(CFLAGS) -c thread_pool.c

clean:
	rm -f $(OBJS) $(BENCH_LOCKS_OBJS) $(BENCH_QUEUE_OBJS) $(TARGET) $(BENCH_LOCKS) $(BENCH_QUEUE)
#define _GNU_SOURCE
#include "mylock.h"
#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>
#include <limits.h>
#include <sched.h>

static int futex_wait(atomic_int *addr, int expected) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAIT,
                   expected, NULL, NULL, 0);
}

static int futex_wake(atomic_int *addr) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAKE,
                   1, NULL, NULL, 0);
}


static _Thread_local mcs_node_t *local_mcs_node = NULL;

void mylock_set_mcs_node(mcs_node_t *node) {
    local_mcs_node = node;
}

void mylock_init(mylock_t *l) {
    mylock_init_kind(l, MYLOCK_SPIN);
}

void mylock_init_kind(mylock_t *l, mylock_kind_t kind) {
    l->kind = kind;

    atomic_flag_clear(&l->spin_flag);

    atomic_init(&l->next_ticket, 0);
    atomic_init(&l->now_serving, 0);

    atomic_init(&l->futex_state, 0);

    atomic_store(&l->mcs_tail, NULL);
}

void mylock_acquire(mylock_t *l) {
    if (l->kind == MYLOCK_SPIN) {
        while (atomic_flag_test_and_set_explicit(&l->spin_flag,
                                                 memory_order_acquire)) {
        }
    } else if (l->kind == MYLOCK_TICKET) {
        unsigned my_ticket =
            atomic_fetch_add_explicit(&l->next_ticket, 1,
                                      memory_order_relaxed);

        while (atomic_load_explicit(&l->now_serving,
                            memory_order_acquire) != my_ticket) {
            sched_yield();
        }
    } else if (l->kind == MYLOCK_FUTEX) {
        int expected = 0;

        if (atomic_compare_exchange_strong_explicit(&l->futex_state,
                                                    &expected,
                                                    1,
                                                    memory_order_acquire,
                                                    memory_order_relaxed)) {
            return;
        }

        while (1) {
            expected = 1;

            futex_wait(&l->futex_state, 1);

            expected = 0;
            if (atomic_compare_exchange_strong_explicit(&l->futex_state,
                                                        &expected,
                                                        1,
                                                        memory_order_acquire,
                                                        memory_order_relaxed)) {
                return;
            }
        }
    } else if (l->kind == MYLOCK_MCS) {
        mcs_node_t *node = local_mcs_node;

        atomic_store(&node->next, NULL);
        atomic_store(&node->locked, 1);

        mcs_node_t *pred =
            atomic_exchange_explicit(&l->mcs_tail, node, memory_order_acq_rel);

        if (pred != NULL) {
            atomic_store_explicit(&pred->next, node, memory_order_release);

            while (atomic_load_explicit(&node->locked, memory_order_acquire)) {
            }
        }
    }
}

void mylock_release(mylock_t *l) {
    if (l->kind == MYLOCK_SPIN) {
        atomic_flag_clear_explicit(&l->spin_flag,
                                   memory_order_release);
    } else if (l->kind == MYLOCK_TICKET) {
        atomic_fetch_add_explicit(&l->now_serving, 1,
                                  memory_order_release);
    } else if (l->kind == MYLOCK_FUTEX) {
        atomic_store_explicit(&l->futex_state, 0,
                              memory_order_release);
        futex_wake(&l->futex_state);
    } else if (l->kind == MYLOCK_MCS) {
        mcs_node_t *node = local_mcs_node;
        mcs_node_t *succ = atomic_load_explicit(&node->next, memory_order_acquire);

        if (succ == NULL) {
            mcs_node_t *expected = node;

            if (atomic_compare_exchange_strong_explicit(
                    &l->mcs_tail,
                    &expected,
                    NULL,
                    memory_order_release,
                    memory_order_relaxed)) {
                return;
            }

            while ((succ = atomic_load_explicit(&node->next, memory_order_acquire)) == NULL) {
            }
        }

        atomic_store_explicit(&succ->locked, 0, memory_order_release);
    }
}

void mycond_init(mycond_t *c) {
    atomic_init(&c->waiters, 0);
    atomic_init(&c->seq, 0);
}

void mycond_wait(mycond_t *c, mylock_t *l) {
    int old_seq = atomic_load_explicit(&c->seq, memory_order_relaxed);

    atomic_fetch_add_explicit(&c->waiters, 1, memory_order_relaxed);

    mylock_release(l);

    futex_wait(&c->seq, old_seq);

    mylock_acquire(l);

    atomic_fetch_sub_explicit(&c->waiters, 1, memory_order_relaxed);
}

void mycond_signal(mycond_t *c) {
    if (atomic_load_explicit(&c->waiters, memory_order_relaxed) > 0) {
        atomic_fetch_add_explicit(&c->seq, 1, memory_order_release);
        futex_wake(&c->seq);
    }
}

void mycond_broadcast(mycond_t *c) {
    if (atomic_load_explicit(&c->waiters, memory_order_relaxed) > 0) {
        atomic_fetch_add_explicit(&c->seq, 1, memory_order_release);

        syscall(SYS_futex,
                (int *)&c->seq,
                FUTEX_WAKE,
                INT_MAX,
                NULL,
                NULL,
                0);
    }
}
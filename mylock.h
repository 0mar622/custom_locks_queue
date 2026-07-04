#ifndef MYLOCK_H
#define MYLOCK_H

#include <stdatomic.h>

// Locking options
typedef enum {
    MYLOCK_SPIN,
    MYLOCK_TICKET,
    MYLOCK_FUTEX,
    MYLOCK_MCS
} mylock_kind_t;

typedef struct mcs_node {
    _Atomic(struct mcs_node *) next;
    atomic_int locked;
} mcs_node_t;

typedef struct mylock {
    mylock_kind_t kind;

    atomic_flag spin_flag;

    atomic_uint next_ticket;
    atomic_uint now_serving;

    atomic_int futex_state;

    _Atomic(mcs_node_t *) mcs_tail;
} mylock_t;

typedef struct mycond {
    atomic_int waiters;
    atomic_int seq;
} mycond_t;  


void mylock_init(mylock_t *l);
void mylock_init_kind(mylock_t *l, mylock_kind_t kind);

void mylock_acquire(mylock_t *l);
void mylock_release(mylock_t *l);

void mycond_init(mycond_t *c);
void mycond_wait(mycond_t *c, mylock_t *l);
void mycond_signal(mycond_t *c);
void mycond_broadcast(mycond_t *c);

void mylock_set_mcs_node(mcs_node_t *node);

#endif
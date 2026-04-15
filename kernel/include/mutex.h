#ifndef CREST_MUTEX_H
#define CREST_MUTEX_H

#include "task.h"

/* A mutex: only one task can hold it at a time.
 * Tasks that call mutex_lock() while it is held are put to TASK_WAITING
 * and stored in a wait list. mutex_unlock() wakes the next waiter.
 */
typedef struct {
    struct TaskControlBlock *owner;     /* task currently holding the mutex, NULL if free */
    struct TaskControlBlock *wait_list; /* singly-linked list of waiting tasks (via ->next) */
} mutex_t;

/* Initialise a mutex to the unlocked state */
void mutex_init(mutex_t *m);

/* Acquire the mutex. Blocks (yields) until the mutex is free.
 * Must NOT be called from an ISR. */
void mutex_lock(mutex_t *m);

/* Release the mutex. Wakes the next task in wait_list if any.
 * Must NOT be called from an ISR. */
void mutex_unlock(mutex_t *m);

#endif /* CREST_MUTEX_H */

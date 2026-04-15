#ifndef CREST_SEMAPHORE_H
#define CREST_SEMAPHORE_H

#include "task.h"

/* A counting semaphore.
 * sem_wait() decrements count; blocks if count == 0.
 * sem_post() increments count; wakes a waiter if count was 0.
 * sem_post() IS safe to call from an ISR (no blocking).
 */
typedef struct {
    int count;                          /* current count (>= 0) */
    struct TaskControlBlock *wait_list; /* tasks blocked in sem_wait() */
} semaphore_t;

/* Initialise with a starting count (e.g. 0 for a signal, N for a resource pool) */
void sem_init(semaphore_t *s, int initial_count);

/* Decrement count; block if count == 0. Must NOT be called from an ISR. */
void sem_take(semaphore_t *s);

/* Increment count and wake one waiter. Safe to call from an ISR. */
void sem_give(semaphore_t *s);

#endif /* CREST_SEMAPHORE_H */

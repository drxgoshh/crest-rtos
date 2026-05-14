#ifndef CREST_SEMAPHORE_H
#define CREST_SEMAPHORE_H

#include "task.h"

/* Counting semaphore:
 *  - `sem_take()` decrements the count; blocks if count == 0.
 *  - `sem_give()` increments the count; wakes a waiter if present.
 * `sem_give()` is safe to call from an ISR.
 */
typedef struct semaphore semaphore_t;

/* Create a new semaphore with `initial_count`. Returns pointer or NULL. */
semaphore_t *sem_create(int initial_count);

/* Decrement count; block if zero. Must NOT be called from an ISR. */
void sem_take(semaphore_t *s);

/* Increment count and wake one waiter. Safe to call from an ISR. */
void sem_give(semaphore_t *s);

#endif /* CREST_SEMAPHORE_H */

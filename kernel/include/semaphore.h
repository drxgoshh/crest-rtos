#ifndef CREST_SEMAPHORE_H
#define CREST_SEMAPHORE_H

#include <stdint.h>
#include "task.h"

/* Counting semaphore:
 *  - `sem_take()` decrements the count; blocks if count == 0.
 *  - `sem_give()` increments the count; wakes a waiter if present.
 * `sem_give()` is safe to call from an ISR.
 */
typedef struct semaphore semaphore_t;

/* Create a new semaphore with `initial_count`. Returns a handle (KOBJ_INVALID on failure). */
uint32_t sem_create(int initial_count);

/* Decrement count; block if zero. Must NOT be called from an ISR. */
void sem_take(uint32_t handle);

/* Increment count and wake one waiter. Safe to call from an ISR. */
void sem_give(uint32_t handle);

#endif /* CREST_SEMAPHORE_H */

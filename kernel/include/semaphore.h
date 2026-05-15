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

/* Decrement count; block until count > 0 or timeout expires.
 * timeout_ms == 0              : non-blocking (returns -1 immediately if unavailable).
 * timeout_ms == CREST_WAIT_FOREVER : block indefinitely.
 * timeout_ms == N              : block up to N ms.
 * Returns 0 on success, -1 on timeout / not available.
 * Must NOT be called from an ISR. */
int sem_take(uint32_t handle, uint32_t timeout_ms);

/* Increment count and wake one waiter. Safe to call from an ISR. */
void sem_give(uint32_t handle);

/*
 * Internal (kernel-only) API used by the kernel worker task.
 *
 * z_impl_sem_try_take — attempt a non-blocking take.
 *   Returns  0 : semaphore acquired.
 *   Returns -1 : not available; caller added to wait list (with optional
 *                timeout via delay_ticks / blocking_wait_list).
 */
int z_impl_sem_try_take_timeout(semaphore_t *s, struct TaskControlBlock *caller,
                                uint32_t timeout_ms);

/* Privileged-path blocking take with timeout. */
int  z_impl_sem_take_timeout(semaphore_t *s, uint32_t timeout_ms);

/* Used by kobject/syscall_api plumbing. */
semaphore_t *z_impl_sem_create(int initial_count);
void         z_impl_sem_give(semaphore_t *s);

#endif /* CREST_SEMAPHORE_H */

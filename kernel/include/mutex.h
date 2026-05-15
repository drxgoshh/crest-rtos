#ifndef CREST_MUTEX_H
#define CREST_MUTEX_H

#include <stdint.h>
#include "task.h"

/* Mutex: exclusive owner semantics. Tasks that call `mutex_lock()` while
 * the mutex is held are placed into `TASK_WAITING` and queued on the
 * mutex' wait list. `mutex_unlock()` wakes the next waiter.
 */
typedef struct mutex mutex_t;

/* Allocate a new mutex. Returns a kernel object handle (KOBJ_INVALID on failure). */
uint32_t mutex_create(void);

/* Acquire the mutex; blocks until available or timeout expires.
 * timeout_ms == 0              : non-blocking (returns -1 immediately if unavailable).
 * timeout_ms == CREST_WAIT_FOREVER : block indefinitely.
 * timeout_ms == N              : block up to N ms.
 * Returns 0 on success (including reentrant lock), -1 on timeout / unavailable.
 * Must NOT be called from an ISR.
 */
int mutex_lock(uint32_t handle, uint32_t timeout_ms);

/* Release the mutex and wake one waiter if present. Must NOT be called
 * from an ISR.
 */
void mutex_unlock(uint32_t handle);

/*
 * Internal (kernel-only) API used by the kernel worker task.
 *
 * z_impl_mutex_try_lock — attempt a non-blocking lock on behalf of `caller`.
 *   Returns  0 : acquired (or caller already owns it).
 *   Returns -1 : held by another task; caller added to wait list with
 *                TASK_FLAG_SVC_BLOCKED set.
 *
 * z_impl_mutex_try_lock_timeout — attempt a non-blocking / timeout lock on behalf of `caller`.
 *   Returns  0 : acquired (or caller already owns it).
 *   Returns -1 : held; caller added to wait list (with optional timeout).
 *
 * z_impl_mutex_lock_timeout — privileged-path blocking lock with timeout.
 *
 * z_impl_mutex_unlock_for — unlock on behalf of `owner` (used by the kernel
 *   worker task which is not itself the mutex owner).
 */
int  z_impl_mutex_try_lock_timeout(mutex_t *m, struct TaskControlBlock *caller,
                                    uint32_t timeout_ms);
int  z_impl_mutex_lock_timeout(mutex_t *m, uint32_t timeout_ms);
void z_impl_mutex_unlock_for(mutex_t *m, struct TaskControlBlock *owner);

/* Used by kobject/syscall_api plumbing. */
mutex_t *z_impl_mutex_create(void);

#endif /* CREST_MUTEX_H */

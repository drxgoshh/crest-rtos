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

/* Acquire the mutex; blocks (yields) until the mutex becomes available.
 * Must NOT be called from an ISR.
 */
void mutex_lock(uint32_t handle);

/* Release the mutex and wake one waiter if present. Must NOT be called
 * from an ISR.
 */
void mutex_unlock(uint32_t handle);

#endif /* CREST_MUTEX_H */

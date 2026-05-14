#ifndef CREST_MUTEX_H
#define CREST_MUTEX_H

#include "task.h"

/* Mutex: exclusive owner semantics. Tasks that call `mutex_lock()` while
 * the mutex is held are placed into `TASK_WAITING` and queued on the
 * mutex' wait list. `mutex_unlock()` wakes the next waiter.
 */
typedef struct mutex mutex_t;

/* Allocate a new mutex. Returns pointer or NULL on allocation failure. */
mutex_t *mutex_create(void);

/* Acquire the mutex; blocks (yields) until the mutex becomes available.
 * Must NOT be called from an ISR.
 */
void mutex_lock(mutex_t *m);

/* Release the mutex and wake one waiter if present. Must NOT be called
 * from an ISR.
 */
void mutex_unlock(mutex_t *m);

#endif /* CREST_MUTEX_H */

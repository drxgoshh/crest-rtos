#include "mutex.h"
#include "isr.h"
#include "port.h"
#include <stddef.h>
#include <stdint.h>
#include "task.h"
#include "sched.h"
#include "alloc.h"

struct mutex {
    struct TaskControlBlock *owner;     /* task currently holding the mutex, NULL if free */
    struct TaskControlBlock *wait_list; /* singly-linked list of waiting tasks (via ->next) */
};

mutex_t* z_impl_mutex_create(void)
{
    mutex_t *m = (mutex_t *)malloc(sizeof(mutex_t));
    if (m == NULL) {
        return NULL;
    }
    m->owner = NULL;
    m->wait_list = NULL;
    return m;
}

/*
 * z_impl_mutex_lock_timeout — privileged blocking lock with optional timeout.
 *
 * timeout_ms == 0              : non-blocking, returns -1 immediately if unavailable.
 * timeout_ms == CREST_WAIT_FOREVER : block indefinitely.
 * timeout_ms == N              : block up to N ms, then return -1.
 *
 * Returns 0 on success (including reentrant), -1 on timeout / unavailable.
 */
int z_impl_mutex_lock_timeout(mutex_t *m, uint32_t timeout_ms)
{
    uint32_t pm = enter_critical();
    if (m->owner == scheduler_get_current()) {
        exit_critical(pm);
        return 0;  /* reentrant */
    }
    if (m->owner == NULL) {
        m->owner = scheduler_get_current();
        exit_critical(pm);
        return 0;
    }
    if (timeout_ms == 0) {
        exit_critical(pm);
        return -1;
    }
    struct TaskControlBlock *cur = scheduler_get_current();
    cur->flags    &= ~TASK_FLAG_TIMED_OUT;
    cur->state     = TASK_WAITING;
    cur->wait_next = m->wait_list;
    m->wait_list   = cur;
    if (timeout_ms != CREST_WAIT_FOREVER) {
        cur->delay_ticks         = timeout_ms;
        cur->blocking_wait_list  = &m->wait_list;
    } else {
        cur->delay_ticks         = 0;
        cur->blocking_wait_list  = NULL;
    }
    exit_critical(pm);
    port_trigger_pendsv();   /* blocks until woken by mutex_unlock or sync_tick_all */
    if (cur->flags & TASK_FLAG_TIMED_OUT) {
        cur->flags &= ~TASK_FLAG_TIMED_OUT;
        return -1;
    }
    return 0;  /* mutex_unlock transferred ownership to us */
}

/*
 * z_impl_mutex_try_lock_timeout — non-blocking variant for the kernel worker
 * task (kwork path).  The caller is already in TASK_WAITING + SVC_BLOCKED
 * state (set by the SVC handler before pushing the work item).
 *
 * timeout_ms == 0              : return -1 immediately without blocking.
 * timeout_ms == CREST_WAIT_FOREVER : block indefinitely (no timeout tick).
 * timeout_ms == N              : set up timeout; sync_tick_all() will call
 *                                scheduler_wake_task(caller, -1) on expiry.
 *
 * Returns  0 : acquired (or reentrant); caller: call complete(caller, 0).
 * Returns -1 : not acquired; if timeout_ms == 0 call complete(caller, -1),
 *              otherwise caller stays on wait list until unlock or timeout.
 */
int z_impl_mutex_try_lock_timeout(mutex_t *m, struct TaskControlBlock *caller,
                                   uint32_t timeout_ms)
{
    uint32_t pm = enter_critical();
    if (m->owner == caller) {
        exit_critical(pm);
        return 0;  /* reentrant */
    }
    if (m->owner == NULL) {
        m->owner = caller;
        exit_critical(pm);
        return 0;
    }
    if (timeout_ms == 0) {
        exit_critical(pm);
        return -1;  /* non-blocking: fail immediately */
    }
    /* Block caller on the wait list. */
    caller->wait_next = m->wait_list;
    m->wait_list      = caller;
    if (timeout_ms != CREST_WAIT_FOREVER) {
        caller->delay_ticks        = timeout_ms;
        caller->blocking_wait_list = &m->wait_list;
    } else {
        caller->delay_ticks        = 0;
        caller->blocking_wait_list = NULL;
    }
    exit_critical(pm);
    return -1;
}

void z_impl_mutex_unlock(mutex_t *m)
{
    uint32_t pm = enter_critical();
    struct TaskControlBlock *cur = scheduler_get_current();
    if (m->owner != cur) {
        exit_critical(pm);
        return;
    }
    if (m->wait_list != NULL) {
        struct TaskControlBlock *waiter = m->wait_list;
        m->wait_list              = waiter->wait_next;
        waiter->wait_next         = NULL;
        waiter->delay_ticks       = 0;
        waiter->blocking_wait_list = NULL;
        m->owner                  = waiter;
        exit_critical(pm);
        scheduler_wake_task(waiter, 0);
    } else {
        m->owner = NULL;
        exit_critical(pm);
        port_trigger_pendsv();
    }
}

/*
 * z_impl_mutex_unlock_for — unlock on behalf of `owner` (used by the kernel
 * worker task which is not itself the owner).
 */
void z_impl_mutex_unlock_for(mutex_t *m, struct TaskControlBlock *owner)
{
    uint32_t pm = enter_critical();
    if (m->owner != owner) {
        exit_critical(pm);
        return;
    }
    if (m->wait_list != NULL) {
        struct TaskControlBlock *waiter = m->wait_list;
        m->wait_list              = waiter->wait_next;
        waiter->wait_next         = NULL;
        waiter->delay_ticks       = 0;
        waiter->blocking_wait_list = NULL;
        m->owner                  = waiter;
        exit_critical(pm);
        scheduler_wake_task(waiter, 0);
    } else {
        m->owner = NULL;
        exit_critical(pm);
        port_trigger_pendsv();
    }
}

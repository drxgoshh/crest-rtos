#include "semaphore.h"
#include "isr.h"
#include "port.h"
#include <stddef.h>
#include <stdint.h>
#include "task.h"
#include "sched.h"
#include "alloc.h"

struct semaphore{
    int count;                          /* current count (>= 0) */
    struct TaskControlBlock *wait_list; /* tasks blocked in sem_wait() */
};

semaphore_t *z_impl_sem_create(int initial_count)
{
    semaphore_t *s = (semaphore_t *)malloc(sizeof(semaphore_t));
    if (s == NULL) {
        return NULL;
    }
    s->count = (initial_count >= 0) ? initial_count : 0;
    s->wait_list = NULL;
    return s;
}

/*
 * z_impl_sem_take_timeout — privileged blocking take with optional timeout.
 *
 * timeout_ms == 0              : non-blocking, returns -1 immediately if not available.
 * timeout_ms == CREST_WAIT_FOREVER : block indefinitely.
 * timeout_ms == N              : block up to N milliseconds, then return -1.
 *
 * Returns 0 on success, -1 on timeout or if non-blocking and unavailable.
 *
 * Implementation: one-shot block.  If the semaphore is not available we add
 * ourselves to the wait list and sleep.  sync_tick_all() splices us out on
 * timeout and sets TASK_FLAG_TIMED_OUT; sem_give() splices us out on success
 * and clears TASK_FLAG_TIMED_OUT (it was never set).  After waking we check
 * the flag to determine success or failure.
 */
int z_impl_sem_take_timeout(semaphore_t *s, uint32_t timeout_ms)
{
    uint32_t pm = enter_critical();
    if (s->count > 0) {
        s->count--;
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
    cur->wait_next = s->wait_list;
    s->wait_list   = cur;
    if (timeout_ms != CREST_WAIT_FOREVER) {
        cur->delay_ticks         = timeout_ms;
        cur->blocking_wait_list  = &s->wait_list;
    } else {
        cur->delay_ticks         = 0;
        cur->blocking_wait_list  = NULL;
    }
    exit_critical(pm);
    port_trigger_pendsv();   /* blocks until woken by sem_give or sync_tick_all */
    /* On return: check why we were woken. */
    if (cur->flags & TASK_FLAG_TIMED_OUT) {
        cur->flags &= ~TASK_FLAG_TIMED_OUT;
        return -1;
    }
    return 0;  /* sem_give transferred ownership to us */
}

/*
 * z_impl_sem_try_take_timeout — non-blocking variant used by the kernel worker
 * task (kwork path).  The caller is already in TASK_WAITING + SVC_BLOCKED
 * state (set by the SVC handler before pushing the work item).
 *
 * timeout_ms == 0              : return -1 immediately without blocking.
 * timeout_ms == CREST_WAIT_FOREVER : block indefinitely (no timeout tick).
 * timeout_ms == N              : set up timeout; sync_tick_all() will call
 *                                scheduler_wake_task(caller, -1) on expiry.
 *
 * Returns  0 : semaphore acquired; caller: call complete(caller, 0).
 * Returns -1 : not acquired; if timeout_ms == 0 call complete(caller, -1),
 *              otherwise caller stays on wait list until sem_give or timeout.
 */
int z_impl_sem_try_take_timeout(semaphore_t *s, struct TaskControlBlock *caller,
                                uint32_t timeout_ms)
{
    uint32_t pm = enter_critical();
    if (s->count > 0) {
        s->count--;
        exit_critical(pm);
        return 0;
    }
    if (timeout_ms == 0) {
        exit_critical(pm);
        return -1;   /* non-blocking: fail immediately */
    }
    /* Block caller on the wait list. */
    caller->wait_next = s->wait_list;
    s->wait_list      = caller;
    if (timeout_ms != CREST_WAIT_FOREVER) {
        caller->delay_ticks        = timeout_ms;
        caller->blocking_wait_list = &s->wait_list;
    } else {
        caller->delay_ticks        = 0;
        caller->blocking_wait_list = NULL;
    }
    exit_critical(pm);
    return -1;
}

void z_impl_sem_give(semaphore_t *s)
{
    uint32_t pm = enter_critical();
    if (s->wait_list != NULL) {
        struct TaskControlBlock *waiter = s->wait_list;
        s->wait_list              = waiter->wait_next;
        waiter->wait_next         = NULL;
        waiter->delay_ticks       = 0;
        waiter->blocking_wait_list = NULL;
        exit_critical(pm);
        scheduler_wake_task(waiter, 0);
        return;
    }
    s->count++;
    exit_critical(pm);
}

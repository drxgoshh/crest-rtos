/* kwork.c — kernel worker task and work-item ring buffer
 *
 * This module implements the kernel side of the SVC-to-kwork pipeline:
 *
 *   1. SVC handler captures frame args, sets caller WAITING, calls kwork_push().
 *   2. kwork_push() inserts a work_item and wakes the kernel task if sleeping.
 *   3. kernel_task_fn() drains the queue, dispatches each item, and calls
 *      scheduler_wake_task() to return results to the original caller.
 *
 * Stack layout (established by PendSV context_switch.c):
 *   tcb->stack_pointer → [R4,R5,R6,R7,R8,R9,R10,R11]  (indices 0..7)
 *                         [R0,R1,R2,R3,R12,LR,PC,xPSR] (indices 8..15)
 *   tcb->stack_pointer[8] = stacked R0 = SVC return value written by
 *   scheduler_wake_task() when TASK_FLAG_SVC_BLOCKED is set.
 */
#include "kwork.h"
#include "sched.h"
#include "port.h"
#include "isr.h"
#include "kobject.h"
#include "queue.h"
#include "semaphore.h"
#include "mutex.h"
#include <stddef.h>
#include <stdint.h>

extern int          z_impl_queue_push(queue_t *q, const void *item, uint32_t ms);
extern int          z_impl_queue_pop(queue_t *q, void *item, uint32_t ms);
extern queue_t     *z_impl_queue_create(uint32_t item_size, uint32_t num_items);
extern void         z_impl_sem_give(semaphore_t *s);
extern int          z_impl_sem_try_take_timeout(semaphore_t *s, struct TaskControlBlock *caller,
                                                uint32_t timeout_ms);
extern semaphore_t *z_impl_sem_create(int initial_count);
extern int          z_impl_mutex_try_lock_timeout(mutex_t *m, struct TaskControlBlock *caller,
                                                   uint32_t timeout_ms);
extern void         z_impl_mutex_unlock_for(mutex_t *m, struct TaskControlBlock *owner);
extern mutex_t     *z_impl_mutex_create(void);
extern void         uart_write(const char *s);

/* ── Work-item ring buffer ─────────────────────────────────────────────── */

static struct work_item kwork_buf[KWORK_QUEUE_SIZE];
static volatile uint32_t kwork_head = 0;
static volatile uint32_t kwork_tail = 0;

/* Pointer to the kernel task's TCB, set at task startup. */
static struct TaskControlBlock *kernel_tcb = NULL;

int kwork_push(const struct work_item *item)
{
    uint32_t pm = enter_critical();

    uint32_t next = (kwork_tail + 1u) % KWORK_QUEUE_SIZE;
    if (next == kwork_head) {
        /* Queue full — should not happen (capacity = MAX_TASKS * 2). */
        exit_critical(pm);
        return -1;
    }

    kwork_buf[kwork_tail] = *item;
    kwork_tail = next;

    /* Wake the kernel task if it is waiting for work. */
    if (kernel_tcb && kernel_tcb->state == TASK_WAITING) {
        kernel_tcb->state = TASK_READY;
    }

    exit_critical(pm);
    return 0;
}

/* Pop one item under a caller-held critical section.
 * Returns 0 on success, -1 if empty. */
static int kwork_pop_locked(struct work_item *out)
{
    if (kwork_head == kwork_tail) return -1;
    *out = kwork_buf[kwork_head];
    kwork_head = (kwork_head + 1u) % KWORK_QUEUE_SIZE;
    return 0;
}

/* ── Result delivery ──────────────────────────────────────────────────── */

static void complete(struct TaskControlBlock *task, uint32_t result)
{
    if (!task) return;
    scheduler_wake_task(task, result);
}

/* ── Syscall dispatch ─────────────────────────────────────────────────── */

static void dispatch(const struct work_item *item)
{
    struct TaskControlBlock *caller = item->caller;

    switch (item->id) {

    case SYSCALL_DELAY:
        /*
         * Pre-write 0 into the saved R0 so the task returns 0 from
         * task_delay() when scheduler_tick() eventually wakes it.
         * Clear TASK_FLAG_SVC_BLOCKED before sleeping so that
         * scheduler_tick()'s plain state=READY wakeup does not try to
         * write to the frame a second time via scheduler_wake_task().
         */
        if (caller) {
            caller->stack_pointer[8] = 0u;
            caller->flags &= ~TASK_FLAG_SVC_BLOCKED;
            scheduler_sleep(caller, item->args[0]);
        }
        break;

    case SYSCALL_QUEUE_SEND: {
        queue_t *q = (queue_t *)k_obj_get((kobj_handle_t)item->args[0], KOBJ_QUEUE);
        if (!q) { complete(caller, (uint32_t)-1); break; }
        int r = z_impl_queue_push(q, (const void *)(uintptr_t)item->args[1], 0u);
        complete(caller, (uint32_t)r);
        break;
    }

    case SYSCALL_QUEUE_RECV: {
        queue_t *q = (queue_t *)k_obj_get((kobj_handle_t)item->args[0], KOBJ_QUEUE);
        if (!q) { complete(caller, (uint32_t)-1); break; }
        int r = z_impl_queue_pop(q, (void *)(uintptr_t)item->args[1], 0u);
        complete(caller, (uint32_t)r);
        break;
    }

    case SYSCALL_SEM_POST: {
        semaphore_t *s = (semaphore_t *)k_obj_get((kobj_handle_t)item->args[0], KOBJ_SEM);
        if (!s) { complete(caller, (uint32_t)-1); break; }
        z_impl_sem_give(s);      /* wakes a waiter if present */
        complete(caller, 0u);    /* wake the sem_give caller */
        break;
    }

    case SYSCALL_SEM_WAIT: {
        semaphore_t *s = (semaphore_t *)k_obj_get((kobj_handle_t)item->args[0], KOBJ_SEM);
        if (!s) { complete(caller, (uint32_t)-1); break; }
        uint32_t timeout_ms = item->args[1];
        int r = z_impl_sem_try_take_timeout(s, caller, timeout_ms);
        if (r == 0) {
            complete(caller, 0u);
        } else if (timeout_ms == 0) {
            complete(caller, (uint32_t)-1);
        }
        /* r == -1, timeout_ms > 0: caller is on the wait list.
         * sem_give() or sync_tick_all() will deliver the result. */
        break;
    }

    case SYSCALL_MUTEX_LOCK: {
        mutex_t *m = (mutex_t *)k_obj_get((kobj_handle_t)item->args[0], KOBJ_MUTEX);
        if (!m) { complete(caller, (uint32_t)-1); break; }
        uint32_t timeout_ms = item->args[1];
        int r = z_impl_mutex_try_lock_timeout(m, caller, timeout_ms);
        if (r >= 0) {
            complete(caller, 0u);
        } else if (timeout_ms == 0) {
            complete(caller, (uint32_t)-1);
        }
        /* r == -1, timeout_ms > 0: caller is on the mutex wait list.
         * mutex_unlock() or sync_tick_all() will deliver the result. */
        break;
    }

    case SYSCALL_MUTEX_UNLOCK: {
        mutex_t *m = (mutex_t *)k_obj_get((kobj_handle_t)item->args[0], KOBJ_MUTEX);
        if (!m) { complete(caller, (uint32_t)-1); break; }
        /* Use _for() variant: the kernel task is not the mutex owner. */
        z_impl_mutex_unlock_for(m, caller);
        complete(caller, 0u);
        break;
    }

    case SYSCALL_UART_WRITE:
        uart_write((const char *)(uintptr_t)item->args[0]);
        complete(caller, 0u);
        break;

    case SYSCALL_QUEUE_CREATE: {
        queue_t *q = z_impl_queue_create(item->args[0], item->args[1]);
        uint32_t h = q ? k_obj_alloc(q, KOBJ_QUEUE) : KOBJ_INVALID;
        complete(caller, h);
        break;
    }

    case SYSCALL_SEM_CREATE: {
        semaphore_t *s = z_impl_sem_create((int)item->args[0]);
        uint32_t h = s ? k_obj_alloc(s, KOBJ_SEM) : KOBJ_INVALID;
        complete(caller, h);
        break;
    }

    case SYSCALL_MUTEX_CREATE: {
        mutex_t *m = z_impl_mutex_create();
        uint32_t h = m ? k_obj_alloc(m, KOBJ_MUTEX) : KOBJ_INVALID;
        complete(caller, h);
        break;
    }

    default:
        complete(caller, (uint32_t)-1);
        break;
    }
}

/* ── Kernel worker task ───────────────────────────────────────────────── */

void kernel_task_fn(void *arg)
{
    (void)arg;

    /* Record our own TCB so kwork_push() can wake us. */
    kernel_tcb = scheduler_get_current();

    for (;;) {
        /* Drain the work queue, releasing the critical section between items
         * so that dispatch() (which may call port_trigger_pendsv) can run
         * without deadlock. */
        while (1) {
            struct work_item item;
            uint32_t pm = enter_critical();
            int got = kwork_pop_locked(&item);
            exit_critical(pm);
            if (got != 0) break;
            dispatch(&item);
        }

        kernel_tcb->state = TASK_WAITING;
        port_trigger_pendsv();
    }
}

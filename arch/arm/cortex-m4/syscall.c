/* syscall.c — SVC handler for Cortex-M4 (kwork-queue edition)
 *
 * The SVC handler no longer dispatches syscalls inline.  It captures the
 * caller's exception frame arguments, sets the caller to WAITING, posts a
 * work_item to the kwork ring buffer, and triggers PendSV.  All real work is
 * done by the kernel worker task (kernel_task_fn in kernel/kwork.c).
 *
 * Exception frame (pushed by hardware on SVC entry, readable via PSP/MSP):
 *   frame[0] R0  — arg0
 *   frame[1] R1  — arg1
 *   frame[2] R2  — arg2
 *   frame[3] R3  — arg3
 *   frame[4] R12 — syscall id (set by port_syscall_invoke before SVC)
 *   frame[5] LR
 *   frame[6] PC
 *   frame[7] xPSR
 *
 * EXC_RETURN bit 2: 0 = MSP, 1 = PSP.
 */
#include <stdint.h>
#include "syscall.h"
#include "port.h"
#include "isr.h"
#include "sched.h"
#include "task.h"
#include "kwork.h"

extern void uart_write(const char *s);

void svc_handler_c(uint32_t *frame);

__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "push {lr}\n"
        "bl svc_handler_c\n"
        "pop {lr}\n"
        "bx lr\n"
    );
}

void svc_handler_c(uint32_t *frame)
{
    syscall_id_t id = (syscall_id_t)frame[4]; /* stacked R12 */

    struct TaskControlBlock *caller = scheduler_get_current();
    if (!caller) {
        /* Kernel not yet running — should not happen. */
        return;
    }

    /* Build work item from the stacked argument registers. */
    struct work_item w = {
        .id     = id,
        .args   = { frame[0], frame[1], frame[2], frame[3] },
        .caller = caller,
    };

    /* Mark caller as blocked via SVC path so scheduler_wake_task() will
     * write the result into frame[0] (stacked R0 = tcb->stack_pointer[8]). */
    uint32_t pm = enter_critical();
    caller->flags |= TASK_FLAG_SVC_BLOCKED;
    caller->state  = TASK_WAITING;
    exit_critical(pm);

    if (kwork_push(&w) != 0) {
        /* Queue full (should never happen): unblock caller with error. */
        uint32_t pm2 = enter_critical();
        caller->flags &= ~TASK_FLAG_SVC_BLOCKED;
        caller->state  = TASK_READY;
        frame[0]       = (uint32_t)-1;
        exit_critical(pm2);
        return;
    }

    /* PendSV will switch away from the caller (WAITING) to the kernel task
     * (READY, highest priority).  kwork_push() already set kernel task READY
     * if it was sleeping. */
    port_trigger_pendsv();
}


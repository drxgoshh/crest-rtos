/*
 * kwork.h — kernel worker task and work-item queue
 *
 * All blocking kernel operations that were previously dispatched inline in
 * the SVC handler are instead posted here as work items.  The kernel worker
 * task (highest priority, privilege level) drains the queue and performs the
 * actual work outside of any ISR context.
 *
 * Result passing:
 *   When the kernel task completes a work item it calls scheduler_wake_task()
 *   which, for SVC-blocked tasks (TASK_FLAG_SVC_BLOCKED), writes the result
 *   into tcb->stack_pointer[8] — the stacked R0 in the hardware exception
 *   frame — so the task resumes with the correct value in R0.
 */
#ifndef CREST_KWORK_H
#define CREST_KWORK_H

#include <stdint.h>
#include "syscall.h"
#include "task.h"

/* Ring-buffer capacity.  One entry per task maximum (one blocked SVC at a
 * time per task), times two for bursts.  Cannot overflow from the SVC path
 * because each task can have at most one outstanding SVC at a time. */
#define KWORK_QUEUE_SIZE (MAX_TASKS * 2)

struct work_item {
    syscall_id_t             id;       /* which syscall to process        */
    uint32_t                 args[4];  /* frame[0..3] captured at SVC     */
    struct TaskControlBlock *caller;   /* task that issued the SVC        */
};

/*
 * kwork_push — enqueue a work item from the SVC handler (or an ISR).
 *
 * Called with interrupts NOT yet re-enabled (SVC handler context).
 * Returns 0 on success, -1 if the queue is full (should not happen in
 * normal operation given KWORK_QUEUE_SIZE == MAX_TASKS * 2).
 */
int kwork_push(const struct work_item *item);

/*
 * kernel_task_fn — entry function for the kernel worker task.
 *
 * Create this task at priority 0 (highest) before calling
 * port_start_first_task().  It runs with full privilege and processes
 * all work items posted by kwork_push().
 */
void kernel_task_fn(void *arg);

#endif /* CREST_KWORK_H */

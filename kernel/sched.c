#include "sched.h"
#include "isr.h"
#include <string.h>
#include <stdint.h>

/*
 * Per-priority task lists.  All tasks at a given priority live in a
 * singly-linked list whose head is task_list[priority].  Tasks are never
 * moved between lists; only their state field changes.
 */
static struct TaskControlBlock *task_list[MAX_TASK_PRIORITIES];

/*
 * Round-robin cursor per priority.  Points to the task that was most
 * recently selected for execution at that priority.  scheduler_get_next()
 * starts its search from the NEXT node so every READY task gets a turn
 * before any task runs twice.
 */
static struct TaskControlBlock *rr_current[MAX_TASK_PRIORITIES];

/* The task currently occupying the CPU (or NULL before first task starts). */
static struct TaskControlBlock *current_task = NULL;

/* Millisecond tick counter — incremented by scheduler_tick() on every SysTick. */
static volatile uint32_t tick_count = 0;

/* ------------------------------------------------------------------ */

void sched_init(void)
{
    memset(task_list,  0, sizeof(task_list));
    memset(rr_current, 0, sizeof(rr_current));
    current_task = NULL;
}

void sched_add_task(struct TaskControlBlock *tcb)
{
    uint32_t pm = enter_critical();
    tcb->next = task_list[tcb->priority];
    task_list[tcb->priority] = tcb;
    exit_critical(pm);
}

void sched_remove_task(struct TaskControlBlock *tcb)
{
    uint32_t pm = enter_critical();

    /* If the round-robin cursor pointed to this task, clear it so the next
     * call to scheduler_get_next() restarts from the head. */
    if (rr_current[tcb->priority] == tcb) {
        rr_current[tcb->priority] = NULL;
    }

    struct TaskControlBlock **p = &task_list[tcb->priority];
    while (*p) {
        if (*p == tcb) {
            *p = tcb->next;
            tcb->next = NULL;
            break;
        }
        p = &(*p)->next;
    }
    exit_critical(pm);
}

/* ------------------------------------------------------------------ */

/*
 * scheduler_get_next — strict-priority round-robin selection.
 *
 * For each priority level (0 = highest) we walk the list starting from the
 * task AFTER rr_current[pr] (the last task that ran at this priority).
 * The list is treated as circular: we wrap from the last node back to
 * task_list[pr].  We stop as soon as we find a TASK_READY node and update
 * rr_current so the next call skips past it.
 *
 * If no task is READY at this priority we continue to the next lower
 * priority.  NULL is returned only when no task is ready anywhere.
 */
struct TaskControlBlock *scheduler_get_next(void)
{
    uint32_t pm = enter_critical();

    for (int pr = 0; pr < MAX_TASK_PRIORITIES; pr++) {
        struct TaskControlBlock *head = task_list[pr];
        if (!head) continue;

        /* Start from the node after the last scheduled one */
        struct TaskControlBlock *start = rr_current[pr] ? rr_current[pr] : head;
        struct TaskControlBlock *t = start->next ? start->next : head;

        /* Walk at most the full list once */
        struct TaskControlBlock *stop = t; /* sentinel: stop when we get back here */
        while (1) {
            if (t->state == TASK_READY) {
                rr_current[pr] = t;
                exit_critical(pm);
                return t;
            }
            t = t->next ? t->next : head; /* wrap */
            if (t == stop) break;
        }
        /* No READY task at this priority; fall through to next */
    }

    exit_critical(pm);
    return NULL;
}

/* ------------------------------------------------------------------ */

struct TaskControlBlock *scheduler_get_current(void)
{
    return current_task;
}

void scheduler_set_current(struct TaskControlBlock *tcb)
{
    current_task = tcb;
}

/* ------------------------------------------------------------------ */

/*
 * scheduler_tick — called from SysTick ISR once per millisecond.
 *
 * Decrements delay_ticks for every TASK_WAITING task.  When the counter
 * reaches zero the task transitions back to TASK_READY so
 * scheduler_get_next() can pick it up on the next PendSV.
 */
uint32_t scheduler_get_tick_count(void)
{
    return tick_count;
}

void scheduler_tick(void)
{
    tick_count++;
    for (int pr = 0; pr < MAX_TASK_PRIORITIES; pr++) {
        struct TaskControlBlock *t = task_list[pr];
        while (t) {
            uint32_t pm = enter_critical();
            if (t->state == TASK_WAITING && t->delay_ticks > 0) {
                if (--t->delay_ticks == 0) {
                    t->state = TASK_READY;
                }
            }
            exit_critical(pm);
            t = t->next;
        }
    }
}

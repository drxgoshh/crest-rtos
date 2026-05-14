#include "sched.h"
#include "port.h"
#include "isr.h"
#include <string.h>
#include <stdint.h>
#include "queue.h"
#include "../boards/stm32f446/uart.h"
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

uint8_t g_priority_mask = 0; /* bitmask of priorities with at least one READY task */

extern queue_t* g_queue_list; /* global list of all queues for cleanup (not implemented) */



/* ------------------------------------------------------------------ */

uint8_t scheduler_get_first_ready_priority(void)
{
    if (g_priority_mask == 0) return MAX_TASK_PRIORITIES; /* no ready tasks */
    return __builtin_ctz(g_priority_mask);
}


void scheduler_init(void)
{
    memset(task_list,  0, sizeof(task_list));
    memset(rr_current, 0, sizeof(rr_current));
    g_priority_mask = 0;
    current_task = NULL;
}

/* Put a task to sleep for ticks milliseconds.
 * Task stays in its priority list — scheduler_get_next skips TASK_WAITING.
 * scheduler_tick() wakes it when delay_ticks reaches zero. */
void scheduler_sleep(struct TaskControlBlock *tcb, uint32_t ticks)
{
    uint32_t pm = enter_critical();
    tcb->delay_ticks = ticks;
    tcb->state       = TASK_WAITING;
    exit_critical(pm);
}

void scheduler_add_task(struct TaskControlBlock *tcb)
{
    uint32_t pm = enter_critical();
    tcb->next = task_list[tcb->priority];
    task_list[tcb->priority] = tcb;
    g_priority_mask |= (1 << tcb->priority);
    port_trigger_pendsv();
    exit_critical(pm);
}

void scheduler_remove_task(struct TaskControlBlock *tcb)
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

    /* If the list is now empty, clear the corresponding bit in the priority mask */
    if (!task_list[tcb->priority]) {
        g_priority_mask &= ~(1 << tcb->priority);
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

    /* Scan all priorities in order (0 = highest). Tasks may be WAITING
     * at any priority, so we cannot rely on g_priority_mask alone. */
    for (uint8_t pr = 0; pr < MAX_TASK_PRIORITIES; pr++) {
        struct TaskControlBlock *list = task_list[pr];
        if (!list) continue;

        /* Round-robin: start from the node after the last one selected. */
        struct TaskControlBlock *start = rr_current[pr] ? rr_current[pr] : list;
        struct TaskControlBlock *t = start;
        do {
            if (t->state == TASK_READY) {
                rr_current[pr] = t->next ? t->next : list;
                exit_critical(pm);
                return t;
            }
            t = t->next ? t->next : list;
        } while (t != start);
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

    /* Walk every priority list and decrement sleeping tasks.          *
     * Tasks stay in the list; only their state changes.              */
    for (uint8_t pr = 0; pr < MAX_TASK_PRIORITIES; pr++) {
        for (struct TaskControlBlock *t = task_list[pr]; t; t = t->next) {
            if (t->state == TASK_WAITING && t->delay_ticks > 0) {
                if (--t->delay_ticks == 0) {
                    t->state = TASK_READY;
                }
            }
        }
    }

    /* Decrement timeouts on queue wait lists */
    queue_tick_all();
}

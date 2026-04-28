/*
 * sched.h — CREST RTOS scheduler API
 *
 * The scheduler owns the per-priority task lists and round-robin state.
 * It is intentionally separate from task lifecycle (task_create/delete/delay)
 * so it can be swapped for a different policy (EDF, rate-monotonic, etc.)
 * without touching task.c.
 *
 * Portability note: sched.c may call enter_critical/exit_critical (from
 * isr.h) but it never touches hardware registers directly. All hardware
 * triggers go through port.h.
 */
#ifndef CREST_SCHED_H
#define CREST_SCHED_H

#include "task.h"

/*
 * sched_init — clear all scheduler state (called once from task_init).
 */
void sched_init(void);

/*
 * sched_add_task — insert a TCB into the ready list for its priority.
 * Called by task_create after the TCB is fully initialised.
 */
void sched_add_task(struct TaskControlBlock *tcb);

/*
 * sched_remove_task — remove a TCB from its priority list.
 * Called by task_delete before the TCB/stack are freed.
 */
void sched_remove_task(struct TaskControlBlock *tcb);

/*
 * scheduler_get_next — pick the next READY task to run.
 *
 * Policy: strict priority (low number = high priority) with round-robin
 * fairness among tasks at the same priority. Returns NULL only if no
 * READY task exists at any priority (should not happen with an idle task).
 */
struct TaskControlBlock *scheduler_get_next(void);

/* Current-task accessors — used by port layer and sync primitives. */
struct TaskControlBlock *scheduler_get_current(void);
void                     scheduler_set_current(struct TaskControlBlock *tcb);

/*
 * scheduler_tick — age delay counters and wake tasks whose delay expired.
 * Must be called once per system tick (from SysTick ISR).
 */
void scheduler_tick(void);

/* Returns the number of milliseconds elapsed since the scheduler started. */
uint32_t scheduler_get_tick_count(void);

/* Get the priority of the first ready task (lowest number = highest priority). */
uint8_t sched_get_first_ready_priority(void);

extern uint8_t g_priority_mask; /* bitmask of priorities with ready tasks */

#endif /* CREST_SCHED_H */

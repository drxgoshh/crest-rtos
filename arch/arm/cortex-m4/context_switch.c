#include "port.h"
#include "task.h"
#include "sched.h"
#include "critical.h"
#include <stdint.h>

/* C helper called from the PendSV assembly wrapper.
 * - input: r0 = pointer to process stack (PSP) after pushing R4-R11
 * - return: r0 = pointer to new process stack (PSP) for the next task (after R4-R11 area)
 */
uint32_t *port_switch_context(uint32_t *old_sp)
{
	struct TaskControlBlock *cur = scheduler_get_current();
	if (cur) {
		cur->stack_pointer = old_sp;
		/* Mark preempted task ready so the scheduler can pick it again.
		 * Tasks blocked via task_delay/mutex/semaphore are already
		 * TASK_WAITING — their state must not be overwritten here. */
		if (cur->state == TASK_RUNNING) cur->state = TASK_READY;
	}

	struct TaskControlBlock *next = scheduler_get_next();
	if (!next) {
		/* No ready task — should not happen if an idle task is registered.
		 * Fall back to re-running the current task. */
		if (cur) { cur->state = TASK_RUNNING; return cur->stack_pointer; }
		return old_sp;
	}

	next->state = TASK_RUNNING;
	scheduler_set_current(next);
	return next->stack_pointer;
}

/* Naked PendSV handler: save callee-saved registers, call the C context-switch
 * helper, then restore registers and return to thread mode. This uses a small
 * inline-asm wrapper but keeps the scheduler logic in C.
 */
void PendSV_Handler(void) __attribute__((naked));
void PendSV_Handler(void)
{
	__asm volatile(
		"MRS R0, PSP\n"            /* R0 = process stack pointer of current task */
		"STMDB R0!, {R4-R11}\n"   /* push callee-saved regs onto process stack */
		"PUSH {LR}\n"              /* save EXC_RETURN on MSP (we are in handler mode) */
		"BL port_switch_context\n" /* R0 in = old SP, R0 out = new SP */
		"POP {LR}\n"               /* restore EXC_RETURN */
		"LDMIA R0!, {R4-R11}\n"   /* pop callee-saved regs from new task stack */
		"MSR PSP, R0\n"           /* set PSP to new task's stack pointer */
		"BX LR\n"                  /* exception return -> restores hw frame from PSP */
		::: "r0", "r1", "r2", "r3", "memory");
}

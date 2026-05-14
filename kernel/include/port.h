/*
 * port.h — CREST RTOS architecture port interface
 *
 * This header defines the interface that the kernel requires from the
 * architecture port. Implementations live under arch/<arch>/<cpu>/port.c.
 *
 * The kernel and sync primitives include THIS header — never an arch path
 * directly. To target a new architecture, provide a new port.c that
 * implements these functions and link it in place of the existing one.
 */
#ifndef CREST_PORT_H
#define CREST_PORT_H

#ifndef PORT_STACK_GUARD_SIZE
#define PORT_STACK_GUARD_SIZE 256u /* bytes of guard region at bottom of stack */
#endif

/* Compile-time toggle to enable/disable MPU configuration in the port.
 * Set to 0 to skip MPU programming for debugging. */
#ifndef PORT_USE_MPU
#define PORT_USE_MPU 1
#endif

/* Forward declaration: avoid including task.h here to keep port.h lightweight. */
struct TaskControlBlock;


/*
 * port_trigger_pendsv — request a context switch.
 *
 * Sets the PendSV pending bit so the context-switch handler fires as soon
 * as the current exception (or critical section) exits. Safe to call from
 * both thread mode and ISRs.
 */
/* Request a context switch by setting the PendSV pending bit. Safe from
 * both thread mode and ISRs.
 */
void port_trigger_pendsv(void);

/*
 * port_start_first_task — enter multitasking mode.
 *
 * Selects the first task (via scheduler_get_next), sets up the initial
 * PSP/CONTROL register state, and returns directly into the task.
 * Never returns.
 */
/* Start the first task and enter multitasking. Does not return. */
void port_start_first_task(void) __attribute__((noreturn));


/* Configure the MPU for the given task. */
void port_mpu_configure_for_task(struct TaskControlBlock *tcb);

/* Called when a stack overflow is detected on context switch.
 * Prints task name, SP, stack_base, and halts. Never returns. */
void stack_overflow_handler(struct TaskControlBlock *tcb) __attribute__((noreturn));

/*
 * Syscall / privilege interface
 * ─────────────────────────────
 * port_syscall_invoke(id, arg0..arg3)
 *   Emit the architecture syscall instruction (SVC #id on ARM) to elevate
 *   from unprivileged thread mode to the kernel syscall handler.
 *   The return value is the value placed by the handler in R0.
 *
 * port_set_unprivileged()
 *   Clear CONTROL.nPRIV = 1 (ARM) so the calling thread drops to user mode.
 *   Call this once from port_start_first_task / port_switch_context for
 *   tasks marked TASK_USER, just before the ERET that enters the task.
 *
 * port_is_privileged()
 *   Returns non-zero if the CPU is currently in privileged thread mode.
 *   Used by syscall stubs to decide whether to trap or call directly.
 */
int  port_syscall_invoke(unsigned int id,
                         unsigned int arg0, unsigned int arg1,
                         unsigned int arg2, unsigned int arg3);
void port_set_unprivileged(void);
int  port_is_privileged(void);
void k_uart_write(const char *s); /* UART write safe from unprivileged mode */

#endif /* CREST_PORT_H */

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

/*
 * port_trigger_pendsv — request a context switch.
 *
 * Sets the PendSV pending bit so the context-switch handler fires as soon
 * as the current exception (or critical section) exits. Safe to call from
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
void port_start_first_task(void) __attribute__((noreturn));

#endif /* CREST_PORT_H */

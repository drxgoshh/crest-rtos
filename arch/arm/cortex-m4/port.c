#include "port.h"
#include "task.h"
#include "sched.h"
#include "critical.h"
#include <stdint.h>

/* SCB ICSR register for PendSV set (ARM Cortex-M) — also defined in
 * critical.h; redefined here so port.c compiles stand-alone if needed. */
#ifndef SCB_ICSR
#define SCB_ICSR           (*(volatile uint32_t *)0xE000ED04)
#define SCB_ICSR_PENDSVSET (1UL << 28)
#endif

void port_trigger_pendsv(void)
{
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
}

void port_start_first_task(void) __attribute__((noreturn));
void port_start_first_task(void)
{
    struct TaskControlBlock *first = scheduler_get_current();
    if (!first) first = scheduler_get_next();
    if (!first) {
        /* No task was created — spin rather than returning to caller */
        while (1) ;
    }

    scheduler_set_current(first);
    first->state = TASK_RUNNING;

    /* Restore the fake stack frame laid out by stack_init() and branch into
     * the task.  After setting CONTROL=2 the processor uses PSP for pop in
     * thread mode, so the pops below consume the software frame (R4-R11) and
     * then the hardware frame (R0-R3,R12,LR,PC,xPSR). */
    uint32_t *sp = first->stack_pointer;
    __asm volatile (
        "msr psp, %0\n"          /* PSP = start of task's software frame  */
        "movs r1, #2\n"          /* CONTROL.SPSEL = 1 → use PSP           */
        "msr control, r1\n"
        "isb\n"
        "pop {r4-r11}\n"         /* restore callee-saved regs from PSP    */
        "pop {r0-r3,r12,lr}\n"   /* restore R0-R3, R12, LR from PSP       */
        "pop {pc}\n"             /* jump to task entry (PC from PSP)      */
        :: "r" (sp) : "r1", "memory");

    while (1) ;
}

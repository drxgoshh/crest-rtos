#include "port.h"
#include "task.h"
#include "sched.h"
#include "critical.h"
#include "registers.h"
#include <stdint.h>
extern void systick_init(void);
extern void systick_enable_irq(void);
extern void uart_write(const char *s);

static void uart_write_hex(uint32_t val)
{
    const char hex[] = "0123456789abcdef";
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 8; i++)
        buf[2 + i] = hex[(val >> ((7 - i) * 4)) & 0xF];
    buf[10] = '\0';
    uart_write(buf);
}

void stack_overflow_handler(struct TaskControlBlock *tcb);

/* SCB_ICSR already defined in registers.h; guard against redefinition */
#ifndef SCB_ICSR
#define SCB_ICSR           (*(volatile uint32_t *)0xE000ED04)
#define SCB_ICSR_PENDSVSET (1UL << 28)
#endif

void stack_overflow_handler(struct TaskControlBlock *tcb)
{
    uart_write("\r\n[CREST] *** STACK OVERFLOW ***\r\n");
    uart_write("  task     : ");
    uart_write(tcb->name);
    uart_write("\r\n");
    uart_write("  sp       : ");
    uart_write_hex((uint32_t)(uintptr_t)tcb->stack_pointer);
    uart_write("\r\n");
    uart_write("  base     : ");
    uart_write_hex((uint32_t)(uintptr_t)tcb->stack_base);
    uart_write("\r\n");
    uart_write("  overflow : ");
    uart_write_hex((uint32_t)((uintptr_t)tcb->stack_base -
                              (uintptr_t)tcb->stack_pointer));
    uart_write(" bytes\r\n");
    uart_write("  Halting.\r\n");
    __asm volatile ("cpsid i");  /* mask all interrupts */
    while (1) {
        __asm volatile ("bkpt #0");
    }
}

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

#if PORT_USE_MPU
    port_mpu_configure_for_task(first);
#endif
    scheduler_set_current(first);
    first->state = TASK_RUNNING;

    /* Set up the SysTick counter and enable its interrupt now that MPU
     * and task stacks are fully configured.  Interrupts are still globally
     * masked (PRIMASK=1 from the bootloader), so the SysTick IRQ cannot
     * fire until cpsie i below. */
    systick_init();

    /* Restore the fake stack frame laid out by stack_init() and branch
     * into the first task.  After CONTROL=2 the CPU uses PSP for pops in
     * thread mode, consuming the software frame (R4-R11) then the
     * hardware frame (R0-R3, R12, LR, PC, xPSR). */
    uint32_t *sp = first->stack_pointer;
    __asm volatile (
        "msr psp, %0\n"          /* PSP = task stack pointer              */
        "movs r1, #2\n"          /* CONTROL.SPSEL = 1 → use PSP           */
        "msr control, r1\n"
        "isb\n"
        "pop {r4-r11}\n"         /* restore callee-saved registers        */
        "pop {r0-r3,r12,lr}\n"   /* restore argument / scratch registers  */
        "cpsie i\n"              /* unmask interrupts — SysTick can fire  */
        "pop {pc}\n"             /* jump to task entry point              */
        :: "r" (sp) : "r1", "memory");

    while (1) ;
}

void port_mpu_configure_for_task(struct TaskControlBlock *tcb){
    if (!tcb || !tcb->stack_base || tcb->stack_size == 0) return;

    const uint32_t guard = PORT_STACK_GUARD_SIZE;
    /* guard must be power-of-two and at least 32 bytes for Cortex-M */
    if ((guard & (guard - 1)) != 0 || guard < 32) return;

    uintptr_t guard_base = (uintptr_t)tcb->stack_base - guard;
    /* Ensure base is aligned to the guard size (allocation should guarantee this) */
    guard_base &= ~(uintptr_t)(guard - 1);

    const uint32_t region = 7; /* reserve MPU region 7 for the guard */

    uint32_t pm = enter_critical();

    /* Save and disable MPU while programming region */
    uint32_t old_ctrl = MPU->CTRL;
    MPU->CTRL = 0;
    __asm volatile ("dsb 0xF" ::: "memory");
    __asm volatile ("isb 0xF" ::: "memory");

    /* Program guard region: select region, set base and attributes */
    MPU->RNR  = region;
    MPU->RBAR = (uint32_t)guard_base;

    /* SIZE field encoding: (log2(region_size) - 1). Use builtin ctz for power-of-two. */
    unsigned int log2 = __builtin_ctz(guard); /* e.g. guard=32 -> log2=5 */
    unsigned int size_field = (log2 > 0) ? (log2 - 1) : 0;

    /* RASR: ENABLE (bit0), SIZE bits[5:1], AP bits[26:24] = 0 (no access), XN bit28 = 1 */
    uint32_t rasr = (1U << 0)                 /* ENABLE */
                 | (size_field << 1)         /* SIZE */
                 | (0U << 24)                /* AP = 0 => no access */
                 | (1U << 28);               /* XN = execute never */

    MPU->RASR = rasr;

    /* Re-enable MPU: ENABLE | PRIVDEFENA (privileged code falls back to
     * default memory map — without this all non-region memory is no-access) */
    MPU->CTRL = old_ctrl | MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm volatile ("dsb 0xF" ::: "memory");
    __asm volatile ("isb 0xF" ::: "memory");

    exit_critical(pm);
}

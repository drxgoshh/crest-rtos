#include "port.h"
#include "task.h"
#include "sched.h"
#include "critical.h"
#include "registers.h"
#include <stdint.h>
extern void systick_init(void);
extern void systick_enable_irq(void);
extern void uart_write(const char *s);

/* Linker-provided user heap symbols (defined in linker script) */
extern uint8_t __user_heap_start[];
extern uint8_t __user_heap_end[];

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
        while (1) ;
    }

#if PORT_USE_MPU
    port_mpu_configure_for_task(first);
#endif
    scheduler_set_current(first);
    first->state = TASK_RUNNING;

    systick_init();

    uint32_t *sp = first->stack_pointer;
    __asm volatile (
        "msr psp, %0\n"
        "movs r1, #2\n"
        "msr control, r1\n"
        "isb\n"
        "pop {r4-r11}\n"
        "pop {r0-r3,r12,lr}\n"
        "cpsie i\n"
        "pop {pc}\n"
        :: "r" (sp) : "r1", "memory");

    while (1) ;
}

void port_mpu_configure_for_task(struct TaskControlBlock *tcb){
    if (!tcb || !tcb->stack_base || tcb->stack_size == 0) return;

    const uint32_t guard = PORT_STACK_GUARD_SIZE;
    if ((guard & (guard - 1)) != 0 || guard < 32) return;

    uintptr_t guard_base = (uintptr_t)tcb->stack_base - guard;
    guard_base &= ~(uintptr_t)(guard - 1);

    /* MPU regions:
     *  r0: Flash 512KB  @ 0x08000000 — user RX
     *  r1: RAM   128KB  @ 0x20000000 — user NO_ACCESS
     *  r2: task stack              — user RW (overrides r1)
     *  r3: user heap               — user RW (overrides r1)
     *  r7: stack guard             — NO_ACCESS (overrides r2)
     */
    const uint32_t region_flash      = 0;
    const uint32_t region_kernel_ram = 1;
    const uint32_t region_stack      = 2;
    const uint32_t region_user_heap  = 3;
    const uint32_t region_guard      = 7;

    uint32_t pm = enter_critical();

    /* Save and disable MPU while programming region */
    uint32_t old_ctrl = MPU->CTRL;
    MPU->CTRL = 0;
    __asm volatile ("dsb 0xF" ::: "memory");
    __asm volatile ("isb 0xF" ::: "memory");

    /* r0: flash — user RX */
    MPU->RNR  = region_flash;
    MPU->RBAR = 0x08000000U;
    MPU->RASR = (1U << 0) | (18U << 1) | (MPU_AP_PRIV_RW_USER_RO << 24);

    /* r1: kernel RAM — user NO_ACCESS */
    MPU->RNR  = region_kernel_ram;
    MPU->RBAR = 0x20000000U;
    MPU->RASR = (1U << 0) | (16U << 1) | (MPU_AP_PRIV_RW_USER_NO << 24) | (1U << 28);

    /* r2: task stack — user RW */
    uintptr_t stack_base = (uintptr_t)tcb->stack_base;
    uintptr_t stack_size = (uintptr_t)tcb->stack_size;
    uintptr_t stack_rbar = stack_base & ~(stack_size - 1);
    unsigned int stack_size_field = __builtin_ctz(stack_size) - 1;
    MPU->RNR  = region_stack;
    MPU->RBAR = (uint32_t)stack_rbar;
    MPU->RASR = (1U << 0) | (stack_size_field << 1) | (MPU_AP_PRIV_RW_USER_RW << 24) | (1U << 28);

    /* r3: user heap — user RW */
    if ((uintptr_t)__user_heap_end > (uintptr_t)__user_heap_start) {
        uintptr_t uh_base = (uintptr_t)__user_heap_start;
        uintptr_t uh_size = (uintptr_t)__user_heap_end - (uintptr_t)__user_heap_start;
        uintptr_t uh_rbar = uh_base & ~(uh_size - 1);
        unsigned int uh_size_field = __builtin_ctz(uh_size) - 1;
        MPU->RNR  = region_user_heap;
        MPU->RBAR = (uint32_t)uh_rbar;
        MPU->RASR = (1U << 0) | (uh_size_field << 1) | (MPU_AP_PRIV_RW_USER_RW << 24) | (1U << 28);
    }

    /* r7: stack guard — NO_ACCESS */
    unsigned int guard_size_field = __builtin_ctz(guard) - 1;
    MPU->RNR  = region_guard;
    MPU->RBAR = (uint32_t)guard_base;
    MPU->RASR = (1U << 0) | (guard_size_field << 1) | (MPU_AP_NO_ACCESS << 24) | (1U << 28);

    MPU->CTRL = old_ctrl | MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm volatile ("dsb 0xF" ::: "memory");
    __asm volatile ("isb 0xF" ::: "memory");

    exit_critical(pm);
}

/* ── Privilege / syscall interface ─────────────────────────────────────── */

void port_set_unprivileged(void)
{
    struct TaskControlBlock *cur = scheduler_get_current();
    if (cur) cur->flags |= TASK_FLAG_USER;
    uint32_t ctrl;
    __asm volatile ("mrs %0, control" : "=r" (ctrl));
    ctrl |= (1u << 0);
    __asm volatile ("msr control, %0\n isb" :: "r" (ctrl) : "memory");
}

int port_is_privileged(void)
{
    uint32_t ctrl;
    __asm volatile ("mrs %0, control" : "=r" (ctrl));
    return ((ctrl & (1u << 0)) == 0);
}

/* id → R12 (stacked by hardware at frame[4]); args shift into R0-R2. */
__attribute__((naked))
int port_syscall_invoke(unsigned int id,
                        unsigned int arg0, unsigned int arg1,
                        unsigned int arg2, unsigned int arg3)
{
    __asm volatile (
        "mov r12, r0\n"
        "mov r0, r1\n"
        "mov r1, r2\n"
        "mov r2, r3\n"
        "svc #0\n"
        "bx lr\n"
    );
}

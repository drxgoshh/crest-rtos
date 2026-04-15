#include <stdint.h>
#include "registers.h"
#include "task.h"
#include "sched.h"
#include "libc_stubs.h"
extern uint32_t _estack;
extern uint32_t _sdata, _edata, _sidata; /* _sidata = flash load address of .data */
extern uint32_t _sbss, _ebss;

#define PREEMPT_TICK_INTERVAL 50  /* trigger PendSV every 50 ms */

extern int main(void);
extern void scheduler_tick(void);
extern void port_trigger_pendsv(void);

/* PendSV handler is implemented in the arch port (context switch code) */
extern void PendSV_Handler(void);



void systick_init(void) {
    SYST_RVR = 16000 - 1;   // 16MHz HSI / 1000 = 16000 ticks per ms
    SYST_CVR = 0;            // clear current value, forces reload
    SYST_CSR = (1 << 2)      // clock source = processor clock
             | (1 << 1)      // enable interrupt
             | (1 << 0);     // enable SysTick
}

void Reset_Handler(void) {
    /* Copy .data section from flash to RAM */
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero .bss */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Enable FPU (CP10/CP11 full access) — required for -mfloat-abi=hard */
    *(volatile uint32_t *)0xE000ED88 |= (0xFu << 20);
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    systick_init();
    main();

    while (1) {}
}

extern void uart_write(const char *s);

void Default_Handler(void) {
    /* Dump fault info for debugging */
    uart_write("HARD FAULT!\n");

    /* Determine which stack pointer (MSP/PSP) holds the stacked registers */
    uint32_t lr;
    __asm volatile ("mov %0, lr" : "=r" (lr));
    uint32_t *stacked = NULL;
    if ((lr & 4) == 0) {
        __asm volatile ("mrs %0, msp" : "=r" (stacked));
    } else {
        __asm volatile ("mrs %0, psp" : "=r" (stacked));
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "EXC_RETURN=0x%lx\n", (unsigned long)lr);
    uart_write(buf);

    if (stacked) {
        snprintf(buf, sizeof(buf), "Stack frame @ %p\n", stacked);
        uart_write(buf);
        snprintf(buf, sizeof(buf), "R0=%lx R1=%lx R2=%lx R3=%lx\n",
             (unsigned long)stacked[0], (unsigned long)stacked[1],
             (unsigned long)stacked[2], (unsigned long)stacked[3]);
        uart_write(buf);
        snprintf(buf, sizeof(buf), "R12=%lx LR=%lx PC=%lx xPSR=%lx\n",
             (unsigned long)stacked[4], (unsigned long)stacked[5],
             (unsigned long)stacked[6], (unsigned long)stacked[7]);
        uart_write(buf);
    }

    /* Fault status registers */
    uint32_t cfsr = (*(volatile uint32_t *)0xE000ED28);
    uint32_t hfsr = (*(volatile uint32_t *)0xE000ED2C);
    uint32_t mmfar = (*(volatile uint32_t *)0xE000ED34);
    uint32_t bfar = (*(volatile uint32_t *)0xE000ED38);
    snprintf(buf, sizeof(buf), "CFSR=0x%lx HFSR=0x%lx MMFAR=0x%lx BFAR=0x%lx\n",
             (unsigned long)cfsr, (unsigned long)hfsr,
             (unsigned long)mmfar, (unsigned long)bfar);
    uart_write(buf);

    struct TaskControlBlock *cur = scheduler_get_current();
    if (cur) {
        snprintf(buf, sizeof(buf), "Current task: %s\n SP(%p)\n", cur->name, (void *)cur->stack_pointer);
        uart_write(buf);
    } else {
        uart_write("No current task\n");
    }

    while (1) {
        __asm volatile ("bkpt #0");
    }
}

void SysTick_Handler(void) {
    scheduler_tick();
    if (scheduler_get_tick_count() % PREEMPT_TICK_INTERVAL == 0) {
        port_trigger_pendsv();
    }
}

__attribute__((section(".isr_vector"))) void (* const vector_table[])(void) = {
    (void (*)(void))(&_estack), /* initial MSP                  */
    Reset_Handler,
    Default_Handler,            /* NMI                          */
    Default_Handler,            /* HardFault                    */
    Default_Handler,            /* MemManage                    */
    Default_Handler,            /* BusFault                     */
    Default_Handler,            /* UsageFault                   */
    0, 0, 0, 0,                 /* reserved                     */
    Default_Handler,            /* SVCall                       */
    Default_Handler,            /* DebugMon                     */
    0,                          /* reserved                     */
    PendSV_Handler,
    SysTick_Handler,
};
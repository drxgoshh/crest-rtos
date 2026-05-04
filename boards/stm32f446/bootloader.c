#include <stdint.h>
#include <stddef.h>
#include "uart.h"

#define KERNEL_START_ADDRESS 0x08004000u /* Start of main firmware in flash, after 16K bootloader */
#define SCB_VTOR  (*(volatile uint32_t *)0xE000ED08U) /* 16K bootloader size */
#define RAM_START 0x20000000U
#define RAM_END   0x20020000U  /* 128KB RAM */
static inline void disable_irqs(void) { __asm volatile ("cpsid i" ::: "memory"); }
static inline void set_msp(uint32_t sp)    { __asm volatile ("msr msp, %0" :: "r"(sp) : ); }

void __start(void) {
    uint32_t *vt = (uint32_t *)KERNEL_START_ADDRESS; /* bootloader vector table at start of flash */
    uint32_t sp  = vt[0]; /* initial stack pointer from vector table */
    uint32_t reset = vt[1]; /* reset handler address from vector table */

    uart_init();
    uart_write("[bootloader] started\n");

    if ((sp < RAM_START) || (sp > RAM_END) || (reset == 0xFFFFFFFFU) || (reset == 0x00000000U)) {
        uart_write("[bootloader] kernel invalid or missing\n");
        while (1) {}
    }

    uart_write("[bootloader] jumping to kernel\n");

    disable_irqs();
    SCB_VTOR = KERNEL_START_ADDRESS; /* point vector table to kernel's vector table */
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
    set_msp(sp);
    __asm volatile("bx %0" :: "r"(reset));
    for(;;);
}
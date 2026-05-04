#include <stdint.h>
#include "registers.h"
#include "task.h"
#include "sched.h"
#include "libc_stubs.h"
extern uint32_t _estack;
extern uint32_t _sdata, _edata, _sidata; /* _sidata = flash load address of .data */
extern uint32_t _sbss, _ebss;

extern void __start(void);


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
    __start();

    while (1) {}
}

void Default_Handler(void) {
    while (1){}
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
    Default_Handler,            /* PendSV                       */
    Default_Handler,        /* SysTick                      */
};
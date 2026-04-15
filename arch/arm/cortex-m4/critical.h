/*
 * critical.h — ARM Cortex-M4 critical section implementation
 *
 * Uses PRIMASK to disable/restore all maskable interrupts.
 * enter_critical() returns saved PRIMASK so sections can be nested.
 * exit_critical() restores it, not unconditionally reenables IRQs.
 */
#ifndef CREST_CRITICAL_H
#define CREST_CRITICAL_H

#include <stdint.h>

/* SCB Interrupt Control and State Register (standard Cortex-M, not board-specific) */
#define SCB_ICSR           (*(volatile uint32_t *)0xE000ED04)
#define SCB_ICSR_PENDSVSET (1UL << 28)

static inline uint32_t enter_critical(void)
{
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r" (primask) :: "memory");
    __asm volatile ("cpsid i" ::: "memory");
    return primask;
}

static inline void exit_critical(uint32_t primask)
{
    __asm volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

#endif /* CREST_CRITICAL_H */

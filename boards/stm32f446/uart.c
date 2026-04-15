#include "uart.h"
#include "registers.h"
#include <stdint.h>

/* Critical section helpers: save and restore PRIMASK to disable/enable interrupts */
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

/* Simple blocking USART2 logger (PA2 TX). Assumes PCLK1 ~= 16 MHz.
 * Baud: 115200 -> BRR ~= 139
 */
void uart_init(void)
{
    /* Enable GPIOA and USART2 clocks */
    RCC_AHB1ENR |= (1 << 0);    /* GPIOA */
    RCC_APB1ENR |= (1 << 17);   /* USART2 */

    /* PA2, PA3 -> Alternate function (AF) */
    GPIOA_MODER &= ~((0x3 << (2*2)) | (0x3 << (3*2)));
    GPIOA_MODER |=  (0x2 << (2*2)) | (0x2 << (3*2)); /* AF mode */

    /* AF7 for USART2 on PA2/PA3 (AFRL: 4*pin) */
    GPIOA_AFRL &= ~((0xF << (4*2)) | (0xF << (4*3)));
    GPIOA_AFRL |=  (7 << (4*2)) | (7 << (4*3));

    /* Configure USART2: 8N1, baud ~115200 for 16MHz PCLK1 */
    USART2_CR1 = 0;
    USART2_CR2 = 0;
    USART2_CR3 = 0;

    USART2_BRR = 139; /* approx 16MHz / 115200 */

    /* Enable transmitter and USART */
    USART2_CR1 |= (1 << 3) | (1 << 13); /* TE, UE */
}

void uart_putc(char c)
{
    /* Wait for TXE (bit 7) */
    while ((USART2_SR & (1 << 7)) == 0) ;
    USART2_DR = (uint32_t)(uint8_t)c;
}

void uart_write(const char *s)
{
    /* Disable interrupts during UART write to prevent preemption from
     * interrupting mid-character and leaving the driver in a bad state.
     */
    uint32_t pm = enter_critical();
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
    exit_critical(pm);
}

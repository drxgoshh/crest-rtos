#ifndef STM32F446_UART_H
#define STM32F446_UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_write(const char *s);

#endif /* STM32F446_UART_H */

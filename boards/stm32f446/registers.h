#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000)
#define GPIOA_ODR    (*(volatile uint32_t *)0x40020014)
#define SYST_CSR  (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR  (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR  (*(volatile uint32_t *)0xE000E018)

/* Additional registers for USART2 (PA2=TX, PA3=RX) */
#define RCC_APB1ENR  (*(volatile uint32_t *)0x40023840)
#define GPIOA_AFRL   (*(volatile uint32_t *)0x40020020)

/* USART2 registers (base 0x40004400) */
#define USART2_SR    (*(volatile uint32_t *)0x40004400)
#define USART2_DR    (*(volatile uint32_t *)0x40004404)
#define USART2_BRR   (*(volatile uint32_t *)0x40004408)
#define USART2_CR1   (*(volatile uint32_t *)0x4000440C)
#define USART2_CR2   (*(volatile uint32_t *)0x40004410)
#define USART2_CR3   (*(volatile uint32_t *)0x40004414)

#define NULL ((void *)0)

#define SCB_ICSR           (*(volatile uint32_t *)0xE000ED04)
#define SCB_ICSR_PENDSVSET (1UL << 28)
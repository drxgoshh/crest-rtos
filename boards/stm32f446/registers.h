#define RCC_AHB1ENR  (*(volatile uint32_t *)0x40023830)
#define GPIOA_MODER  (*(volatile uint32_t *)0x40020000)
#define GPIOA_ODR    (*(volatile uint32_t *)0x40020014)
#define SYST_CSR  (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR  (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR  (*(volatile uint32_t *)0xE000E018)
#define SYST_CSR_ENABLE    (1u << 0)  /* SysTick enable            */
#define SYST_CSR_TICKINT   (1u << 1)  /* SysTick interrupt enable  */
#define SYST_CSR_CLKSOURCE (1u << 2)  /* 1 = processor clock       */

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

/* SCB — System Control Block */
#define SCB_ICSR           (*(volatile uint32_t *)0xE000ED04)
#define SCB_ICSR_PENDSVSET (1UL << 28)
#define SCB_CPACR          (*(volatile uint32_t *)0xE000ED88)  /* Coprocessor Access Control */
#define SCB_CPACR_FPU_FULL (0xFu << 20)                        /* CP10/CP11 full access      */
#define SCB_SHCSR          (*(volatile uint32_t *)0xE000ED24)  /* System Handler Control     */
#define SCB_SHCSR_MEMFAULTENA (1u << 16)                       /* Enable MemManage handler   */

/* Fault status registers */
#define SCB_CFSR  (*(volatile uint32_t *)0xE000ED28)  /* Configurable Fault Status  */
#define SCB_HFSR  (*(volatile uint32_t *)0xE000ED2C)  /* HardFault Status           */
#define SCB_MMFAR (*(volatile uint32_t *)0xE000ED34)  /* MemManage Fault Address    */
#define SCB_BFAR  (*(volatile uint32_t *)0xE000ED38)  /* BusFault Address           */
#define SCB_MMFSR (*(volatile uint8_t  *)0xE000ED28)  /* MemManage Fault Status byte */
#define SCB_MMFSR_DACCVIOL (1u << 1)  /* Data access violation      */
#define SCB_MMFSR_MSTKERR  (1u << 4)  /* Stacking error             */

/* MPU registers (base 0xE000ED90) */
#define MPU_BASE  0xE000ED90UL
#ifndef MPU_TYPE_DEFINED
typedef struct {
    volatile uint32_t TYPE;
    volatile uint32_t CTRL;
    volatile uint32_t RNR;
    volatile uint32_t RBAR;
    volatile uint32_t RASR;
} MPU_Type;
#define MPU ((MPU_Type *)MPU_BASE)
#define MPU_TYPE_DEFINED
#endif
#define MPU_CTRL_ENABLE     (1u << 0)  /* MPU enable                 */
#define MPU_CTRL_PRIVDEFENA (1u << 2)  /* Privileged default map     */
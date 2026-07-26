/*
 * helper.c
 *
 * Author: Md. Samiul Islam Siam (02)
 *         Partho Kumar Mondal (07)
 */

#include "helper.h"

void SystemClock_Config(void)
{
    /* Enable HSE and wait */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /*
     * PLL: HSE(8 MHz) / M(4) * N(180) / P(2) = 180 MHz
     */
    RCC->PLLCFGR = (4U   << RCC_PLLCFGR_PLLM_Pos)
                 | (180U << RCC_PLLCFGR_PLLN_Pos)
                 | (0U   << RCC_PLLCFGR_PLLP_Pos)   /* P = 2 */
                 | RCC_PLLCFGR_PLLSRC_HSE;

    /* Flash latency for 180 MHz + enable prefetch/cache */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* AHB=/1, APB1=/4 (45 MHz), APB2=/2 (90 MHz) */
    RCC->CFGR = RCC_CFGR_HPRE_DIV1
              | RCC_CFGR_PPRE1_DIV4
              | RCC_CFGR_PPRE2_DIV2;

    /* Enable PLL and switch SYSCLK */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void UART_Config(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2: AF7 (USART2_TX), high speed */
    GPIOA->MODER   |= (2U << (2 * 2));
    GPIOA->AFR[0]  |= (7U << (2 * 4));
    GPIOA->OSPEEDR |= (3U << (2 * 2));

    /*
     * BRR for 115 200 baud, APB1 = 45 MHz:
     *   USARTDIV  = 45 000 000 / (16 × 115 200) = 24.414
     *   Mantissa  = 24  (0x18)
     *   Fraction  = round(0.414 × 16) = 7
     *   BRR       = (24 << 4) | 7 = 0x187
     */
    USART2->BRR = (24U << 4) | 7U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

void UART_Print(const char *s)
{
    while (*s) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = (uint8_t)(*s++);
    }
}

/* Iterations per millisecond at 180 MHz, ~4 cycles per loop body */
#define CYCLES_PER_MS   45000UL
#define CYCLES_PER_US      45UL

// delay_ms
void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < (ms * CYCLES_PER_MS); i++) {
        __asm("NOP");
    }
}

// delay_us
void delay_us(uint32_t us)
{
    for (uint32_t i = 0; i < (us * CYCLES_PER_US); i++) {
        __asm("NOP");
    }
}

//  Public flag
volatile uint8_t TIM6_Tick_1Hz = 0;

// Internal ms counter (0 – 999)
static volatile uint32_t ms_count = 0;

void TIM6_Init(void)
{
    /* Enable TIM6 peripheral clock */
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    /* Disable counter while configuring */
    TIM6->CR1 = 0;

    /*
     * PSC = 89:
     *   TIM6 clock = 90 MHz  (APB1 × 2, because PPRE1 ≠ 1)
     *   Counter clock = 90 MHz / (89 + 1) = 1 MHz → 1 µs per tick
     *
     * ARR = 999:
     *   Counter runs 0 → 999 → overflow → UIF set every 1 000 µs = 1 ms
     */
    TIM6->PSC = 89U;
    TIM6->ARR = 999U;

    /* Force PSC/ARR into shadow registers before starting */
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR  = 0;              /* Clear UIF raised by UG */

    /* Enable update interrupt */
    TIM6->DIER = TIM_DIER_UIE;

    /* NVIC: TIM6_DAC_IRQn, priority 1 */
    NVIC_SetPriority(TIM6_DAC_IRQn, 1);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    /* Start free-running counter */
    TIM6->CR1 = TIM_CR1_CEN;
}

// IRQ Handler — fires every 1 ms
void TIM6_DAC_IRQHandler(void)
{
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR = 0;           /* Clear UIF immediately */

        ms_count++;
        if (ms_count >= 1000U) {
            ms_count      = 0;
            TIM6_Tick_1Hz = 1;  /* Signal main loop */
        }
    }
}

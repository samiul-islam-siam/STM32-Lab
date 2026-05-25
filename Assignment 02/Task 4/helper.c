/*
 * CSE 2206: Lab-02
 * Task 4 — PWM Generation and LED Brightness Control
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Modnal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <stdint.h>

void SystemClock_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR      |= PWR_CR_VOS;              /* Voltage Scale 1 */

    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    FLASH->ACR  = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |=  FLASH_ACR_LATENCY_5WS;

    /* PLL: 16/8=2 MHz × 180 = 360 MHz VCO / 2 = 180 MHz */
    RCC->PLLCFGR  = 0;
    RCC->PLLCFGR |= (8U   << RCC_PLLCFGR_PLLM_Pos);
    RCC->PLLCFGR |= (180U << RCC_PLLCFGR_PLLN_Pos);
    RCC->PLLCFGR |= (0U   << RCC_PLLCFGR_PLLP_Pos);  /* 00 = /2 */
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI;
    RCC->PLLCFGR |= (2U   << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    PWR->CR |= PWR_CR_ODEN;
    while (!(PWR->CSR & PWR_CSR_ODRDY));
    PWR->CR |= PWR_CR_ODSWEN;
    while (!(PWR->CSR & PWR_CSR_ODSWRDY));

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;    /* AHB  = 180 MHz */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;   /* APB1 =  45 MHz */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;   /* APB2 =  90 MHz */

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/*
 * USART2 — 115200 8N1
 * PA2 = TX (AF7), PA3 = RX (AF7)
 * fAPB1 = 45 MHz → BRR = 0x187
 */
void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 → AF7 TX */
    GPIOA->MODER  &= ~(3U << (2*2));
    GPIOA->MODER  |=  (2U << (2*2));
    GPIOA->AFR[0] &= ~(0xFU << (4*2));
    GPIOA->AFR[0] |=  (7U   << (4*2));

    /* PA3 → AF7 RX */
    GPIOA->MODER  &= ~(3U << (3*2));
    GPIOA->MODER  |=  (2U << (3*2));
    GPIOA->AFR[0] &= ~(0xFU << (4*3));
    GPIOA->AFR[0] |=  (7U   << (4*3));

    USART2->BRR = (24U << 4) | 7U;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void USART2_SendString(const char *s)
{
    while (*s) {
        while (!(USART2->SR & USART_SR_TXE)) {}
        USART2->DR = (uint8_t)(*s++);
    }
    while (!(USART2->SR & USART_SR_TC)) {}
}


volatile uint32_t ms_count = 0; // Global ms counter

/*
 * TIM2 for 1 ms delay interrupt
 *
 * fTIM2_CLK = 90 MHz
 * PSC = 89  → tick = 1 µs
 * ARR = 999 → overflow every 1000 µs = 1 ms
 */
void TIM2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    __NOP();
    __NOP();

    TIM2->CR1  &= ~TIM_CR1_CEN;
    TIM2->PSC   = 89U;
    TIM2->ARR   = 999U;
    TIM2->EGR   = TIM_EGR_UG;
    TIM2->SR    = 0U;
    TIM2->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, 0U);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;
        ms_count++;
    }
}

void delay_ms(uint32_t ms)
{
    uint32_t start = ms_count;
    while ((ms_count - start) < ms) {}
}

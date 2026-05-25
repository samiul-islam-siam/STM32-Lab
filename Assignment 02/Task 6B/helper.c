/*
 * CSE 2206: Lab-02
 * Task 6 - Option B: Servo Motor Control
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Mondal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <string.h>

void SystemClock_Config(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;

	PWR->CR |= PWR_CR_VOS;

	RCC->CR |= RCC_CR_HSION;
	while (!(RCC->CR & RCC_CR_HSIRDY));

	FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_5WS;

	RCC->PLLCFGR = 0;
	RCC->PLLCFGR |= (8 << RCC_PLLCFGR_PLLM_Pos);
	RCC->PLLCFGR |= (180 << RCC_PLLCFGR_PLLN_Pos);
	RCC->PLLCFGR |= (0 << RCC_PLLCFGR_PLLP_Pos);   // PLLP = 2
	RCC->PLLCFGR |= (RCC_PLLCFGR_PLLSRC_HSI);
	RCC->PLLCFGR |= (2 << RCC_PLLCFGR_PLLQ_Pos);

	RCC->CR |= RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY));

	PWR->CR |= PWR_CR_ODEN;
	while (!(PWR->CSR & PWR_CSR_ODRDY));

	PWR->CR |= PWR_CR_ODSWEN;
	while (!(PWR->CSR & PWR_CSR_ODSWRDY));

	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;

	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}


void USART2_Init(void)
{
    RCC->AHB1ENR  |= RCC_AHB1ENR_GPIOAEN;   /* GPIOA clock */
    RCC->APB1ENR  |= RCC_APB1ENR_USART2EN;  /* USART2 clock */

    GPIOA->MODER  &= ~(3U << (2*2));
    GPIOA->MODER  |=  (2U << (2*2));         /* Alternate function */
    GPIOA->AFR[0] &= ~(0xF << (4*2));
    GPIOA->AFR[0] |=  (7U  << (4*2));        /* AF7 = USART2 */

    GPIOA->MODER  &= ~(3U << (3*2));
    GPIOA->MODER  |=  (2U << (3*2));
    GPIOA->AFR[0] &= ~(0xF << (4*3));
    GPIOA->AFR[0] |=  (7U  << (4*3));

	USART2->BRR = (24 << 4) | 7;   // 0x187

    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void USART2_SendString(const char *s)
{
    while (*s)
    {
        while (!(USART2->SR & USART_SR_TXE)) {}   /* Wait TX empty */
        USART2->DR = (uint8_t)(*s++);
    }
    while (!(USART2->SR & USART_SR_TC)) {}         /* Wait TX complete */
}

void TIM6_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    __NOP();
    __NOP();

    TIM6->CR1 &= ~TIM_CR1_CEN;

    TIM6->PSC = 89U;

    TIM6->ARR = 0xFFFFU;

    TIM6->EGR = TIM_EGR_UG;

    TIM6->SR = 0U;

    TIM6->CR1 |= TIM_CR1_CEN;
}

void delay_us(uint16_t us)
{
    /* Reset counter to zero */
    TIM6->CNT = 0U;
    while ((uint16_t)TIM6->CNT < us) {}
}

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        delay_us(1000U);   /* 1 ms = 1000 µs */
    }
}

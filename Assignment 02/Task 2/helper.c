/*
 * CSE 2206: Lab-02
 * Task-02: Delay Generation
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Modnal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <string.h>

/**
 * SYSCLK = 180 MHz
 */
void SystemClock_Config(void) {

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	PWR->CR |= PWR_CR_VOS;


	RCC->CR |= RCC_CR_HSION;
	while (!(RCC->CR & RCC_CR_HSIRDY));


	FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_5WS;

	/* 5. Configure PLL
	 HSI = 16 MHz
	 PLLM = 8
	 PLLN = 180
	 PLLP = 2
	 PLLQ = 2
	 */

	RCC->PLLCFGR = 0;
	RCC->PLLCFGR |= (8 << RCC_PLLCFGR_PLLM_Pos);
	RCC->PLLCFGR |= (180 << RCC_PLLCFGR_PLLN_Pos);
	RCC->PLLCFGR |= (0 << RCC_PLLCFGR_PLLP_Pos);
	RCC->PLLCFGR |= (RCC_PLLCFGR_PLLSRC_HSI); /* Clock Source = HSI*/
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

/**
 * Initialize USART2 for 115200 8N1
 * PA2: TX (AF7), PA3: RX (AF7)
 */
void USART2_Init(void)
{

    RCC->AHB1ENR  |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR  |= RCC_APB1ENR_USART2EN;


    GPIOA->MODER  &= ~(3U << (2*2));
    GPIOA->MODER  |=  (2U << (2*2));
    GPIOA->AFR[0] &= ~(0xF << (4*2));
    GPIOA->AFR[0] |=  (7U  << (4*2));        /* AF7 = USART2 */


    GPIOA->MODER  &= ~(3U << (3*2));
    GPIOA->MODER  |=  (2U << (3*2));
    GPIOA->AFR[0] &= ~(0xF << (4*3));
    GPIOA->AFR[0] |=  (7U  << (4*3));

	/* 3. Baud rate calculation
	   fCK = 45 MHz
	   Baud = 115200
	   USARTDIV = 24.414 (fCK/(16*Baud))
	   Mantissa = 24
	   Fraction ≈ 7
	*/

	USART2->BRR = (24 << 4) | 7;   // 0x187

    /* 5. Enable TX, RX, and the peripheral */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/**
 * Transmit a null-terminated string over USART2
 * s = Pointer to string
 */
void USART2_SendString(const char *s)
{
    while (*s)
    {
        while (!(USART2->SR & USART_SR_TXE)) {}   /* Wait until TXE set */
        USART2->DR = (uint8_t)(*s++);
    }
    while (!(USART2->SR & USART_SR_TC)) {}        /* Wait until TC set */
}

/**
 * Initialize PA5 for LED (LD2)
 */
void LED_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER &= ~(3U << (5*2));
    GPIOA->MODER |=  (1U << (5*2));
}

void LED_On(void)
{
	GPIOA->ODR |=  (1U << 5);
}

void LED_Off(void)
{
	GPIOA->ODR &= ~(1U << 5);
}

void LED_Toggle(void)
{
	GPIOA->ODR ^= (1U << 5);
}

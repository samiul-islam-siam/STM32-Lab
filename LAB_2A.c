/* Lab 2A - UART2 Polling TX/RX (No HAL) on STM32F446RE
 * UART2 : PA2=TX , PA3=RX | APB1 Bus | Baud 115200 , 8N1
 * APB1 clock assumed = 45 MHz ( configure PLL accordingly )
 */

#include <stdint.h>
#include <stm32f446xx.h>

#define APB1_CLK 45000000UL
#define BAUD_RATE 115200UL

void SystemClock_Config(void) {
	/* 1. Enable HSE and wait for it to be ready */
	RCC->CR |= RCC_CR_HSEON;
	while (!(RCC->CR & RCC_CR_HSERDY));

	/* 2. Enable Power Controller clock, set VOS scale 1 */
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	PWR->CR |= PWR_CR_VOS; /* Scale 1: max 180 MHz */

	/* 3. Configure Flash: 5 wait-states + ART Accelerator + prefetch */
	FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;

	/* 4. Configure PLL: HSE/4 * 180 / 2 = 180 MHz */
	RCC->PLLCFGR = (4 << RCC_PLLCFGR_PLLM_Pos) /* PLLM = 4 */
	| (180 << RCC_PLLCFGR_PLLN_Pos) /* PLLN = 180 */
	| (0 << RCC_PLLCFGR_PLLP_Pos) /* PLLP = /2 */
	| (15 << RCC_PLLCFGR_PLLQ_Pos) /* PLLQ = 15 */
	| RCC_PLLCFGR_PLLSRC_HSE; /* Source: HSE */

	/* 5. Set bus prescalers: AHB/1, APB1/4, APB2/2 */
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1
	| RCC_CFGR_PPRE1_DIV4 /* APB1 = 180/4 = 45 MHz */
	| RCC_CFGR_PPRE2_DIV2; /* APB2 = 180/2 = 90 MHz */

	/* 6. Enable PLL and wait */
	RCC->CR |= RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY));

	/* 7. Switch system clock to PLL */
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void USART2_Init(void) {
	/* 1. Enable clocks */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
	__NOP();
	__NOP();

	/* 2. PA2 , PA3 -> Alternate Function 7 ( USART2 ) */
	GPIOA->MODER &= ~((3UL << 4) | (3UL << 6));
	GPIOA->MODER |= ((2UL << 4) | (2UL << 6)); /* AF mode */
	GPIOA->AFR[0] &= ~((0xFUL << 8) | (0xFUL << 12));
	GPIOA->AFR[0] |= ((7UL << 8) | (7UL << 12)); /* AF7 */
	GPIOA->OTYPER &= ~((1UL << 2) | (1UL << 3)); /* Push - Pull */
	GPIOA->OSPEEDR |= ((3UL << 4) | (3UL << 6)); /* Very High */

	/* 3. Baud rate : BRR = 45 ,000 ,000 / 115 ,200 = 390.625
	 * Mantissa = 390 , Fraction = 0.625 * 16 = 10 */
	USART2->BRR = (390 << 4) | 10;

	/* 4. Enable USART , transmitter , receiver */
	USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void USART2_SendChar(char c) {
	while (!(USART2->SR & USART_SR_TXE)); /* wait TX buffer empty */
	USART2->DR = (uint8_t) c;
}

void USART2_SendString(const char *str) {
	while (*str)
		USART2_SendChar(*str++);
}

char USART2_RecvChar(void) {
	while (!(USART2->SR & USART_SR_RXNE)); /* wait data ready */
	return (char) USART2->DR; /* reading DR clears RXNE */
}

int main(void) {
	SystemClock_Config();
	USART2_Init();

	USART2_SendString(" STM32F446RE UART Polling Demo \r\n");
	USART2_SendString(" Type a character -- it will be echoed :\r\n");

	while (1) {
		char c = USART2_RecvChar(); /* blocking receive */
		USART2_SendChar(c); /* echo back */
		if (c == '\r')
			USART2_SendChar('\n');
	}
}

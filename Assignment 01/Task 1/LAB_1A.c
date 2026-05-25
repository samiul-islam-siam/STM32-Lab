/* Lab 1A - LED Blink (No HAL) on STM32F446RE Nucleo
 * PA5 = LD2 ( User LED) on Nucleo - F446RE
 * Clock source : HSI 16 MHz ( default after reset )
 */

#include <stdint.h>
#include <stm32f446xx.h>

void delay_ms ( uint32_t ms) {
	/* Software delay : ~16 MHz HSI -> ~4 ,000 NOP iterations per ms */
	for ( uint32_t i = 0; i < ms * 4000; i++) {
		__NOP ();
	}
 }

 int main ( void ) {
	/* 1. Enable GPIOA clock on AHB1 bus */
	RCC -> AHB1ENR |= RCC_AHB1ENR_GPIOAEN ;
	__NOP (); __NOP (); /* brief settle delay ( errata recommendation ) */

	/* 2. MODER : set PA5 as General Purpose Output ( bits [11:10] = 01) */
	GPIOA -> MODER &= ~(3UL << (5 * 2)); /* clear bits */
	GPIOA -> MODER |= (1UL << (5 * 2)); /* output mode */

	/* 3. OTYPER : Push - Pull ( bit5 = 0, reset default ) */
	GPIOA -> OTYPER &= ~(1UL << 5);

	/* 4. OSPEEDR : Low speed ( bits [11:10] = 00, reset default ) */
	GPIOA -> OSPEEDR &= ~(3UL << (5 * 2));

	/* 5. PUPDR : No pull ( bits [11:10] = 00, reset default ) */
	GPIOA -> PUPDR &= ~(3UL << (5 * 2));

	while (1) {
		GPIOA -> BSRR = (1UL << 5); /* Set PA5 -> LED ON */
		delay_ms (500) ;
		GPIOA -> BSRR = (1UL << (5 + 16)); /* Reset PA5 -> LED OFF */
		delay_ms (500) ;
	}
 }

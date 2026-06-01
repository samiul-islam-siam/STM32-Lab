/*
 * @file    main.c
 * @brief   LCD 1602A 4-bit parallel mode, STM32F446RE
 *
 */

#include "main.h"
#include "lcd.h"

void SystemClock_Config(void);
void GPIO_Init(void);
void image();

Lcd_HandleTypeDef lcd;

int main(void) {
	SystemClock_Config();
	GPIO_Init();

	/* Pin mapping:
	 D4 → PC7   D5 → PB6   D6 → PA7   D7 → PA6
	 RS → PB5   EN → PB4                          */
	Lcd_PortType ports[] = { GPIOC, GPIOB, GPIOA, GPIOA };
	Lcd_PinType pins[] = { GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_6 };

	lcd = Lcd_create(ports, pins,
					 GPIOB, GPIO_PIN_5, /* RS */
					 GPIOB, GPIO_PIN_4, /* EN */
					 LCD_4_BIT_MODE);
	image();

	while (1) {
	}
}

void image() {

	Lcd_clear(&lcd);

	// SIAM
	uint8_t image07[8] = { 0x00, 0x1F, 0x10, 0x1F, 0x01, 0x01, 0x1F, 0x00 };
	uint8_t image08[8] = { 0x00, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x00 };
	uint8_t image09[8] = { 0x00, 0x1F, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00 };
	uint8_t image10[8] = { 0x00, 0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x00 };

	Lcd_define_char(&lcd, 0, image07);
	Lcd_define_char(&lcd, 1, image08);
	Lcd_define_char(&lcd, 2, image09);
	Lcd_define_char(&lcd, 3, image10);

	Lcd_cursor(&lcd, 0, 6);
	Lcd_write_char(&lcd, 0);
	Lcd_cursor(&lcd, 0, 7);
	Lcd_write_char(&lcd, 1);
	Lcd_cursor(&lcd, 0, 8);
	Lcd_write_char(&lcd, 2);
	Lcd_cursor(&lcd, 0, 9);
	Lcd_write_char(&lcd, 3);

}

void SystemClock_Config(void) {
	/* Enable HSI and wait until ready */
	RCC->CR |= RCC_CR_HSION;
	while (!(RCC->CR & RCC_CR_HSIRDY)) {
	}

	/* Select HSI as system clock source */
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_HSI;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
	}

	/* AHB, APB1, APB2 prescalers all = 1 */
	RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
}

void GPIO_Init(void) {
	/* Enable clocks for GPIOA, GPIOB, GPIOC */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

	/* Small delay after enabling clocks (AHB bus sync) */
	volatile uint32_t dummy = RCC->AHB1ENR;
	(void) dummy;

	/* Helper macro: set two MODER bits for a pin to "01" (output) */
	/* MODER field = bits [2n+1 : 2n] for pin n */

	/* PA6, PA7 → output */
	GPIOA->MODER &= ~((3U << (6 * 2)) | (3U << (7 * 2)));
	GPIOA->MODER |= ((1U << (6 * 2)) | (1U << (7 * 2)));
	GPIOA->OTYPER &= ~((1U << 6) | (1U << 7)); /* push-pull */
	GPIOA->OSPEEDR &= ~((3U << (6 * 2)) | (3U << (7 * 2))); /* low speed */
	GPIOA->PUPDR &= ~((3U << (6 * 2)) | (3U << (7 * 2))); /* no pull */
	GPIOA->ODR &= ~((1U << 6) | (1U << 7)); /* reset low */

	/* PB4, PB5, PB6 → output */
	GPIOB->MODER &= ~((3U << (4 * 2)) | (3U << (5 * 2)) | (3U << (6 * 2)));
	GPIOB->MODER |= ((1U << (4 * 2)) | (1U << (5 * 2)) | (1U << (6 * 2)));
	GPIOB->OTYPER &= ~((1U << 4) | (1U << 5) | (1U << 6));
	GPIOB->OSPEEDR &= ~((3U << (4 * 2)) | (3U << (5 * 2)) | (3U << (6 * 2)));
	GPIOB->PUPDR &= ~((3U << (4 * 2)) | (3U << (5 * 2)) | (3U << (6 * 2)));
	GPIOB->ODR &= ~((1U << 4) | (1U << 5) | (1U << 6));

	/* PC7 → output */
	GPIOC->MODER &= ~(3U << (7 * 2));
	GPIOC->MODER |= (1U << (7 * 2));
	GPIOC->OTYPER &= ~(1U << 7);
	GPIOC->OSPEEDR &= ~(3U << (7 * 2));
	GPIOC->PUPDR &= ~(3U << (7 * 2));
	GPIOC->ODR &= ~(1U << 7);
}

void Error_Handler(void) {
	__disable_irq();
	while (1) {
	}
}

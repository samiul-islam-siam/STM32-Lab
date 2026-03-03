/* Lab 4A - UART2 Receive Interrupt + Ring Buffer (No HAL )
 * Receives bytes in ISR into a ring buffer.
 * Main loop detects complete lines and echoes them.
 */

#include <stdint.h>
#include <stm32f446xx.h>
#include <string.h>
#include <stdio.h>

#define BUF_SIZE 128
volatile char ring[BUF_SIZE];
volatile uint8_t head = 0, tail = 0;
volatile uint8_t lineReady = 0;

/* USART2 ISR */
void USART2_IRQHandler(void) {
	if (USART2->SR & USART_SR_RXNE) {
		char c = (char) USART2->DR; /* reading DR clears RXNE */
		ring[head] = c;
		head = (head + 1) % BUF_SIZE;
		if (c == '\r')
			lineReady = 1;
	}
	if (USART2->SR & USART_SR_ORE) {
		(void) USART2->DR; /* reading DR also clears ORE */
	}
}

void USART2_SendString(const char *s) {
	while (*s) {
		while (!( USART2->SR & USART_SR_TXE))
			;
		USART2->DR = (uint8_t) (*s++);
	}
}

int main(void) {
	/* GPIOA + USART2 clock and GPIO AF config ( same as Lab 2A) */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
	__NOP();
	__NOP();
	GPIOA -> MODER &= ~((3UL << 4) |(3UL << 6));
	GPIOA -> MODER |= ((2UL << 4) |(2UL << 6));
	GPIOA ->AFR [0] &= ~((0xFUL << 8) |(0xFUL << 12));
	GPIOA ->AFR [0] |= ((7UL << 8) |(7UL << 12));

	USART2->BRR = (390 << 4) | 10; /* 115200 @ 45 MHz */
	USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE; /* RXNE IRQ en */

	NVIC_SetPriority(USART2_IRQn, 1);
	NVIC_EnableIRQ(USART2_IRQn);

	USART2_SendString("UART Interrupt Demo - type and press Enter:\r\n");

	while (1) {
		if (lineReady) {
			lineReady = 0;
			/* Drain ring buffer into a local line buffer */
			char line[BUF_SIZE];
			uint8_t i = 0;
			while (tail != head) {
				line[i++] = ring[tail];
				tail = (tail + 1) % BUF_SIZE;
			}
			line[i] = '\0';
			char resp[BUF_SIZE + 16];
			snprintf(resp, sizeof(resp), "Echo : %s\r\n", line);
			USART2_SendString(resp);
		}
	}
}

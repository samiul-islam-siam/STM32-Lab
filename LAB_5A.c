/* Lab 5A - UART2 DMA TX ( Stream6 Ch4) + Circular RX ( Stream5 Ch4)
 2 * No HAL -- direct DMA register configuration
 3 */

#include <stdint.h>
#include <stm32f446xx.h>
#include <string.h>

#define RX_BUF_SIZE 64
static char txMsg[] = "STM32 No -HAL DMA UART Transmit Demo\r\n";
static volatile char rxBuf[RX_BUF_SIZE];

void DMA1_Init(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
	__NOP();
	__NOP();

	/* --- TX: DMA1 Stream6 Channel4 ( Memory -> Peripheral ) --- */
	DMA1_Stream6->CR = 0; /* disable first */
	while ( DMA1_Stream6->CR & DMA_SxCR_EN)
		; /* wait disabled */
	DMA1_Stream6->PAR = (uint32_t) &USART2->DR;
	DMA1_Stream6->CR = (4UL << DMA_SxCR_CHSEL_Pos) /* Channel 4 */
	| DMA_SxCR_DIR_0 /* Mem -> Periph */
	| DMA_SxCR_MINC /* Memory increment */
	| DMA_SxCR_TCIE; /* TC interrupt */

	/* --- RX: DMA1 Stream5 Channel4 ( Peripheral -> Memory , Circular ) --- */
	DMA1_Stream5->CR = 0;
	while ( DMA1_Stream5->CR & DMA_SxCR_EN)
		;
	DMA1_Stream5->PAR = (uint32_t) &USART2->DR;
	DMA1_Stream5->M0AR = (uint32_t) rxBuf;
	DMA1_Stream5->NDTR = RX_BUF_SIZE;
	DMA1_Stream5->CR = (4UL << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_CIRC /* Circular mode */
	| DMA_SxCR_MINC;
	DMA1_Stream5->CR |= DMA_SxCR_EN; /* Enable RX DMA */

	NVIC_SetPriority(DMA1_Stream6_IRQn, 1);
	NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

/* TX Transfer Complete ISR */
void DMA1_Stream6_IRQHandler(void) {
	if (DMA1->HISR & DMA_HISR_TCIF6) {
		DMA1->HIFCR = DMA_HIFCR_CTCIF6; /* clear TC flag */
		USART2->CR3 &= ~ USART_CR3_DMAT; /* disable UART DMA TX */
		DMA1_Stream6->CR &= ~ DMA_SxCR_EN;
	}
}

void DMA_UART_Transmit(const char *data, uint16_t len) {
	while ( DMA1_Stream6->CR & DMA_SxCR_EN)
		; /* wait if busy */
	DMA1_Stream6->M0AR = (uint32_t) data;
	DMA1_Stream6->NDTR = len;
	USART2->CR3 |= USART_CR3_DMAT; /* enable UART DMA TX */
	DMA1_Stream6->CR |= DMA_SxCR_EN; /* start transfer */
}
void USART2_DMA_Init(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
	__NOP();
	__NOP();
	GPIOA->MODER &= ~((3UL << 4) | (3UL << 6));
	GPIOA->MODER |= ((2UL << 4) | (2UL << 6));
	GPIOA->AFR[0] &= ~((0xFUL << 8) | (0xFUL << 12));
	GPIOA->AFR[0] |= ((7UL << 8) | (7UL << 12));
	USART2->BRR = (390 << 4) | 10;
	USART2->CR3 |= USART_CR3_DMAR; /* enable UART DMA RX */
	USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

int main(void) {
	USART2_DMA_Init();
	DMA1_Init();
	DMA_UART_Transmit(txMsg, strlen(txMsg)); /* non - blocking TX */

	uint16_t prevNDTR = RX_BUF_SIZE;
	while (1) {
		/* Detect new RX data by monitoring NDTR decrement */
		uint16_t curNDTR = DMA1_Stream5->NDTR;
		if (curNDTR != prevNDTR) {
			/* New data in rxBuf -- process here */
			prevNDTR = curNDTR;
		}
		__WFI();
	}
}

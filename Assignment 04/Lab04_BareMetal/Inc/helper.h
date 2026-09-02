/*
 * CSE 2206 - Lab 04: ADC Multi-Resolution Testing with Flash Data Logging
 *
 */

#ifndef HELPER_H_
#define HELPER_H_

#include <stdint.h>

void SystemClock_Config(void);

void USART2_Init(void);
void USART2_SendString(const char *s);
void USART2_SendChar(char c);
uint8_t USART2_TryReceiveByte(uint8_t *outByte); /* non-blocking, returns 1 if a byte was read */

void TIM2_Init(void);
void delay_ms(uint32_t ms);
uint32_t millis(void); /* current ms_count, for debounce timing */

#endif /* HELPER_H_ */

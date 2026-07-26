/*
 * helper.h
 *
 * Author: Md. Samiul Islam Siam (02)
 *         Partho Kumar Mondal (07)
 */

#ifndef HELPER_H
#define HELPER_H

#include "stm32f446xx.h"
#include <stdint.h>

/* Clock & peripherals */
void SystemClock_Config(void);
void UART_Config(void);

/* UART output */
void UART_Print(const char *s);

/* Delay */
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

/* TIM6 1 Hz tick */
void TIM6_Init(void);
extern volatile uint8_t TIM6_Tick_1Hz;

#endif /* HELPER_H */

/*
 * helper.h
 *
 * Author: Md. Samiul Islam Siam (02)
 *         Partho Kumar Mondal (07)
 */

#ifndef HELPER_H_
#define HELPER_H_

#include "stm32f446xx.h"
#include <stdint.h>

void SystemClock_Config(void);

void UART_Config(void);
void UART_Print(const char *s);

/* BLOCKING DELAY
 *
 * The loop constant is calibrated for 180 MHz:
 *   Each iteration = ~4 cycles (MOV + SUBS + NOP + BNE)
 *   → CYCLES_PER_MS = 180 000 000 / 4 / 1000 = 45 000
 *   → CYCLES_PER_US = 45 000 / 1000           = 45
 */

/**
 * @brief  Blocking delay using a NOP loop.
 * @param  ms  Approximate delay in milliseconds.
 */
void delay_ms(uint32_t ms);

/**
 * @brief  Blocking delay using a NOP loop.
 * @param  us  Approximate delay in microseconds.
 */
void delay_us(uint32_t us);


/* TIM6 basic timer: 1 Hz interrupt tick
 *
 * Clock tree:
 *   SYSCLK  = 180 MHz
 *   APB1    =  45 MHz  (PPRE1 = /4)
 *   TIM6 clk=  90 MHz  (APB1 prescaler ≠ 1 → timer clock × 2)
 *
 * Configuration:
 *   PSC = 89   → counter clock = 90 MHz / 90 = 1 MHz  (1 µs / tick)
 *   ARR = 999  → overflow every 1 000 µs = 1 ms
 *   ISR increments a millisecond counter; every 1 000th call
 *   sets TIM6_Tick_1Hz = 1 for the main loop to consume.
 */
extern volatile uint8_t TIM6_Tick_1Hz;

/**
 * @brief  Initialise TIM6 and start the 1 ms interrupt.
 *         Enables TIM6_DAC_IRQn in NVIC at priority 1.
 *         TIM6_Tick_1Hz will be set every 1 000 ms after this call.
 */
void TIM6_Init(void);

#endif /* HELPER_H_ */

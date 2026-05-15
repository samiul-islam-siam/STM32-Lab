/*
 * CSE 2206: Lab-02
 * Task-02: Delay Generation
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Modnal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <stdio.h>
#include "helper.h"

/**
 * Initialize TIM6 with 1 µs tick for delay functions
 * TIM_CLK = 2 × APB1 = 90 MHz
 */
void TIM6_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    __NOP();
    __NOP();

    TIM6->CR1 &= ~TIM_CR1_CEN;

    TIM6->PSC = 89U;

    /* Auto-reload = max 16-bit value (65535 µs) */
    TIM6->ARR = 0xFFFFU; // free running

    TIM6->EGR = TIM_EGR_UG;

    TIM6->SR = 0U;

    TIM6->CR1 |= TIM_CR1_CEN;
}

/**
 * Microsecond blocking delay
 * ISR safe operation
 * us = microseconds (Maximum 65535 μs (16-bit counter limit))
 */
void delay_us(uint16_t us)
{
    /* Capture the current count */
	uint16_t start = TIM6->CNT;

    /* Wait until desired count */
	while ((uint16_t)(TIM6->CNT - start) < us) {}
}

/**
 * Millisecond blocking delay
 * ms = milliseconds (32-bit unsigned integer)
 */
void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        delay_us(1000U);   /* 1 ms = 1000 µs */
    }
}

/**
 * Second blocking delay
 * sec = seconds (32-bit unsigned integer)
 */
void delay_s(uint32_t sec)
{
    for (uint32_t i = 0; i < sec; i++)
    {
        delay_ms(1000U);   /* 1 s = 1000 ms */
    }
}

/**
 * Hour/minute/second blocking delay
 * h = Hours
 * m = Minutes
 * s = Seconds
 * each 8-bit unsigned integer
 */
void delay_hms(uint8_t h, uint8_t m, uint8_t s)
{
    /* Use 32-bit accumulator to prevent overflow during multiplication */
    uint32_t total_s = (uint32_t)h * 3600U
                     + (uint32_t)m * 60U
                     + (uint32_t)s;
    delay_s(total_s);
}

int main(void)
{
	char buf[80];

	SystemClock_Config();
    USART2_Init();
    TIM6_Init();
    LED_Init();

    USART2_SendString("\r\n===== Task 2: Delay Generation (Bare-Metal) =====\r\n");

    /* --- Demo 1: delay_ms(500) --- */
    USART2_SendString("Starting 500 ms delay...\r\n");
    delay_ms(500U);
    USART2_SendString("Done. [500 ms elapsed]\r\n\r\n");

    /* --- Demo 2: delay_ms(1000) --- */
    USART2_SendString("Starting 1000 ms delay...\r\n");
    delay_ms(1000U);
    USART2_SendString("Done. [1000 ms elapsed]\r\n\r\n");

    /* --- Demo 3: LED toggle 10 times at 250 ms on / 250 ms off --- */
    USART2_SendString("LED toggle demo: 10 x (250ms ON / 250ms OFF)...\r\n");
    for (int i = 0; i < 10; i++)
    {
        LED_On();
        snprintf(buf, sizeof(buf), "  Toggle %2d/10: LED ON\r\n",  i + 1);
        USART2_SendString(buf);
        delay_ms(250U);

        LED_Off();
        snprintf(buf, sizeof(buf), "  Toggle %2d/10: LED OFF\r\n", i + 1);
        USART2_SendString(buf);
        delay_ms(250U);
    }
    USART2_SendString("LED toggle done.\r\n\r\n");

    /* --- Demo 4: delay_s(3) --- */
    USART2_SendString("Starting 3 s delay...\r\n");
    delay_s(3U);
    USART2_SendString("Done. [3 s elapsed]\r\n\r\n");

    /* --- Demo 5: delay_hms(0, 0, 5) --- */
    USART2_SendString("Starting delay_hms(0, 0, 5)  [5 seconds]...\r\n");
    delay_hms(0U, 0U, 5U);
    USART2_SendString("Done. [5 s elapsed]\r\n\r\n");

    USART2_SendString("===== Task 2 Complete =====\r\n");


    while (1) {}
}

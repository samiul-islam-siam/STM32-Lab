/*
 * CSE 2206: Lab-02
 * Task 4 — PWM Generation and LED Brightness Control
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Modnal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "helper.h"

uint8_t sine_lut[256];

void SineLUT_Init(void)
{
    for (uint16_t i = 0; i < 256; i++)
    {
    	float val = 50.0f * (1.0f + sinf(2.0f * (float)M_PI * i / 256.0f));
        sine_lut[i] = (uint8_t)(val + 0.5f);
    }
}

/*
 * TIM3 PWM — Channel 1 on PA6 (AF2)
 *
 * fTIM3_CLK = 90 MHz
 * PSC = 89   → tick_freq = 1,000,000 Hz (1 µs per tick)
 * ARR = 999  → f_PWM = 1,000,000 / 1000 = 1 kHz
 *
 * PWM Mode 1 (OC1M = 110):
 *   CNT < CCR1 → PA6 HIGH
 *   CNT ≥ CCR1 → PA6 LOW
 *
 * CCR1 = pct × (ARR+1) / 100 = pct × 10
 */
#define TIM3_ARR  999U    /* ARR value — defines PWM period */

void TIM3_PWM_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    __NOP();
    __NOP();

    GPIOA->MODER   &= ~(3U << (6*2));
    GPIOA->MODER   |=  (2U << (6*2));    /* AF mode          */

    GPIOA->OTYPER  &= ~(1U << 6);        /* Push-pull        */
    GPIOA->OSPEEDR |=  (3U << (6*2));    /* Very high speed  */
    GPIOA->PUPDR   &= ~(3U << (6*2));    /* No pull          */

    /*	PA6 → AF2 (TIM3_CH1)*/
    GPIOA->AFR[0] &= ~(0xFU << (4*6));
    GPIOA->AFR[0] |=  (2U   << (4*6));   /* AF2 = TIM3       */

    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->PSC  = 89U;                    /* tick = 1 µs      */
    TIM3->ARR  = TIM3_ARR;               /* period = 1000 µs = 1 ms → 1 kHz */

    // PWM Mode 1 on Channel 1
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |=  TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;  /* 110 */
    TIM3->CCMR1 |=  TIM_CCMR1_OC1PE;

    TIM3->CCR1 = 0U;

    TIM3->CCER |=  TIM_CCER_CC1E;
    TIM3->CCER &= ~TIM_CCER_CC1P;

    TIM3->CR1 |= TIM_CR1_ARPE;

    TIM3->EGR  = TIM_EGR_UG;
    TIM3->SR   = 0U;

    TIM3->CR1 |= TIM_CR1_CEN;
}

/*
 * PWM_SetDuty
 *
 * Accepts pct = 0..100
 * Computes CCR1 = pct × (ARR+1) / 100
 *               = pct × 1000    / 100
 *               = pct × 10
 *
 * Because OC1PE is set, the new CCR1 takes effect at the
 * next Update Event → glitch-free transitions.
 */
void PWM_SetDuty(uint8_t pct)
{
    if (pct > 100U) pct = 100U;           /* clamp */
    TIM3->CCR1 = (uint32_t)pct * (TIM3_ARR + 1U) / 100U;
}


int main(void)
{
    SCB->CPACR |= (0xF << 20); // Enable FPU
    __DSB();
    __ISB();

    char buf[64];

    SystemClock_Config();
    USART2_Init();
    TIM2_Init();        /* ms_count via TIM2 NVIC — no SysTick */

    TIM3_PWM_Init();    /* PA6 PWM @ 1 kHz */
    SineLUT_Init();

    USART2_SendString("===== Task 4: PWM Generation and LED Brightness Control (Bare-Metal) =====\r\n\r\n");

    /*
     * Part 1 — Duty-cycle sweep: 0% to 100% in 10% steps
     * Hold each step for 300 ms
     */
    USART2_SendString("\r\n-- Part 1: Duty-Cycle Sweep --\r\n");
    USART2_SendString("Duty    | CCR1\r\n");
    USART2_SendString("--------|------\r\n");

    for (uint8_t pct = 0; pct <= 100; pct += 10)
    {
        PWM_SetDuty(pct);

        uint32_t ccr1 = (uint32_t)pct * (TIM3_ARR + 1U) / 100U;
        snprintf(buf, sizeof(buf),
                 "Duty = %3u%%  |  CCR1 = %4lu\r\n",
                 pct, (unsigned long)ccr1);
        USART2_SendString(buf);

        delay_ms(300U);
    }

    /*
     * Part 2 — Sine-wave breathing effect (5 cycles)
     * LUT[i] = 50 × (1 + sin(2πi/256))
     * Each step holds 8 ms → full period = 256×8 = 2048 ms
     */
    USART2_SendString("\r\n-- Part 2: Sine Breathing (5 cycles) --\r\n");

    for (uint8_t cycle = 1; cycle <= 5; cycle++)
    {
        snprintf(buf, sizeof(buf), "Breath cycle %u/5\r\n", cycle);
        USART2_SendString(buf);

        for (uint16_t i = 0; i < 256; i++)
        {
            PWM_SetDuty(sine_lut[i]);
            delay_ms(8U);
        }
    }

    /*
     * Part 3 — Final hold at exactly 50%
     */
    PWM_SetDuty(50U);
    uint32_t final_ccr1 = (uint32_t)50U * (TIM3_ARR + 1U) / 100U;
    snprintf(buf, sizeof(buf),
             "\r\n-- Final Hold --\r\nDuty = 50%%   CCR1 = %lu\r\n",
             (unsigned long)final_ccr1);
    USART2_SendString(buf);

    USART2_SendString("\r\n====== Task 4 Complete ======\r\n");

    while (1) {}
}


/*
 * LED:
 * longer leg (positive or anode) : D12 PWM with resistor
 * smaller leg (negative or cathode): GND
 */

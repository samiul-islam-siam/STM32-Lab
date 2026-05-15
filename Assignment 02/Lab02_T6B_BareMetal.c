/*
 * CSE 2206: Lab-02
 * Task 6 - Option B: Servo Motor Control
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Mondal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>
#include "helper.h"

/*
 * PWM on PA0 (AF1):
 * fTIM2 = 90 MHz, PSC = 99, ARR = 17999
 */
#define SERVO_PSC   99U      /* tick = 900 kHz          */
#define SERVO_ARR   17999U   /* period = 20 ms = 50 Hz  */

static void TIM2_Servo_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    __NOP(); __NOP();

    /* PA0: AF1 (TIM2_CH1) */
    GPIOA->MODER  &= ~(3U << (0*2));
    GPIOA->MODER  |=  (2U << (0*2));
    GPIOA->OTYPER &= ~(1U << 0);
    GPIOA->OSPEEDR|=  (3U << (0*2));
    GPIOA->PUPDR  &= ~(3U << (0*2));
    GPIOA->AFR[0] &= ~(0xFU << (4*0));
    GPIOA->AFR[0] |=  (0x1U << (4*0));   /* AF1 = TIM2 */

    TIM2->CR1 &= ~TIM_CR1_CEN;

    TIM2->PSC  = SERVO_PSC; // 99
    TIM2->ARR  = SERVO_ARR; // 17999
    TIM2->CCR1 = 450;       // Start at 0°

    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM2->CCMR1 |= (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
    TIM2->CCER  |= TIM_CCER_CC1E;
    TIM2->CCER  &= ~TIM_CCER_CC1P;
    TIM2->CR1   |= TIM_CR1_ARPE;
    TIM2->EGR    = TIM_EGR_UG;
    TIM2->CR1   |= TIM_CR1_CEN;
}

/*
 * Servo Motor MG996R: 50 Hz (20 ms period)
 * 0°   → pulse = 0.5 ms → CCR = 450
 * 90°  → pulse = 1.5 ms → CCR = 1350
 * 180° → pulse = 2.5 ms → CCR = 2250
 */
static uint32_t Servo_AngleToCCR(uint8_t angle)
{
    // 0° to 180°: 2000 µs total range
    float t_us = 500.0f + ((float)angle / 180.0f) * 2000.0f;

    // CCR = t_us × (900000 / 1000000) = t_us × 0.9f
    return (uint32_t)(t_us * 0.9f);
}

static void FPU_Enable(void)
{
    /* Enable FPU — set CP10 and CP11 full access */
    SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
    __DSB();   /* Data Sync Barrier */
    __ISB();   /* Instruction Sync Barrier */
}

int main(void)
{
    char buf[100];

    FPU_Enable();
    SystemClock_Config();
    USART2_Init();
    TIM6_Init();

    TIM2_Servo_Init();

    USART2_SendString("\r\n===== Task 6B: Servo Motor Control (Bare-Metal) =====\r\n");
    USART2_SendString("Angle | Pulse (us) | CCR\r\n");
    USART2_SendString("------+------------+-----\r\n");

    delay_ms(1000U);   /* Let it stabilize */

    /* Sweep 0° to 180° in 18 steps of 10° */
    for (uint8_t angle = 0; angle <= 180U; angle += 10U)
    {
    	float t_us = 500.0f + ((float)angle / 180.0f) * 2000.0f;
        uint32_t ccr = Servo_AngleToCCR(angle);

        TIM2->CCR1 = ccr;

        snprintf(buf, sizeof(buf),
            "%5u | %10.2f | %4lu\r\n",
            angle, t_us, (unsigned long)ccr);
        USART2_SendString(buf);

        delay_ms(500U);
    }

    /* Return to center */
    TIM2->CCR1 = Servo_AngleToCCR(90U);
    USART2_SendString("\r\nParked at 90 degrees.\r\n");

    USART2_SendString("===== Task 6B Complete =====\r\n");

    while (1) {}
}

/*
 * Servo Motor MG996R
 * Red Wire: Positive --> +5V
 * Brown Wire: Negative --> GND
 * Orange Wire: Signal (PWM) --> A0
 */

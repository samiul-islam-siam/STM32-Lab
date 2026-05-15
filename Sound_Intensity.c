#include "stm32f446xx.h"
#include <stdint.h>

/* =========================
   SYSTEM CLOCK (unchanged)
   ========================= */
void SystemClock_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                 FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;

    RCC->PLLCFGR = 0;
    RCC->PLLCFGR |= (8U << RCC_PLLCFGR_PLLM_Pos);
    RCC->PLLCFGR |= (180U << RCC_PLLCFGR_PLLN_Pos);
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;

    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* =========================
   ADC1 PA0
   ========================= */
void ADC1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    GPIOA->MODER |= (3U << (0 * 2)); // analog mode

    ADC1->CR2 = 0;
    ADC1->SQR3 = 0; // channel 0

    ADC1->CR2 |= ADC_CR2_ADON;

    for (volatile int i = 0; i < 10000; i++);
}

/* =========================
   ADC READ
   ========================= */
uint16_t ADC_Read(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC));
    return ADC1->DR;
}

/* =========================
   PWM LED PA6 TIM3
   ========================= */
void PWM_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    GPIOA->MODER &= ~(3U << (6 * 2));
    GPIOA->MODER |=  (2U << (6 * 2)); // AF

    GPIOA->AFR[0] &= ~(0xF << (6 * 4));
    GPIOA->AFR[0] |=  (2U << (6 * 4));

    TIM3->PSC = 89;
    TIM3->ARR = 999;

    TIM3->CCMR1 |= (6 << 4); // PWM mode
    TIM3->CCER |= TIM_CCER_CC1E;

    TIM3->CR1 |= TIM_CR1_CEN;
}

/* =========================
   LED BRIGHTNESS
   ========================= */
void LED_Set(uint16_t v)
{
    if (v > 999) v = 999;
    TIM3->CCR1 = v;
}

/* =========================
   SIMPLE AVERAGING
   ========================= */
uint16_t readAudioLevel(uint16_t baseline)
{
    uint32_t sum = 0;

    for (int i = 0; i < 32; i++)
    {
        uint16_t v = ADC_Read();
        if (v > baseline) sum += (v - baseline);
        else sum += (baseline - v);
    }

    return sum / 32;
}

/* =========================
   MAIN
   ========================= */
int main(void)
{
    SystemClock_Config();
    ADC1_Init();
    PWM_Init();

    /* -------- calibration -------- */
    uint32_t base = 0;
    for (int i = 0; i < 200; i++)
        base += ADC_Read();

    base /= 200;

    while (1)
    {
        uint16_t level = readAudioLevel(base);

        /* noise filter */
        if (level < 5)
        {
            LED_Set(0);
        }
        else
        {
            if (level > 200) level = 200;

            LED_Set(level * 5); // scale brightness
        }
    }
}

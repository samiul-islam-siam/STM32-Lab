/*
 * ============================================================
 * CSE 2206 — Microcontroller & Embedded System Lab-02
 * Task 4 — PWM Generation and LED Brightness Control (TIM3)
 * Platform : STM32F446RE Nucleo-64
 * Output   : PA6 → TIM3 Channel 1 (AF2) → External LED + 220Ω
 *            PA5 → GPIO OUT → LD2 (timing reference toggle)
 *            PA2 → USART2 TX (AF7) → UART output
 *
 * ── System Clock ───────────────────────────────────────────
 *  HSI(16) / PLLM(8) × PLLN(180) / PLLP(2) = 180 MHz CPU
 *  AHB  = 180 MHz (DIV1)
 *  APB1 =  45 MHz (DIV4)  → TIM3 clock = 45×2 = 90 MHz
 *  APB2 =  90 MHz (DIV2)
 *
 * ── TIM3 PWM @ 1 kHz ───────────────────────────────────────
 *  fTIM3_CLK = 90 MHz
 *  PSC = 89   → tick_freq = 90,000,000 / 90 = 1,000,000 Hz
 *  ARR = 999  → f_PWM = 1,000,000 / 1000 = 1,000 Hz = 1 kHz
 *
 *  CCR1 = pct × (ARR+1) / 100
 *       = pct × 1000    / 100
 *       = pct × 10
 *
 *  Example:
 *   25% → CCR1 = 250
 *   50% → CCR1 = 500
 *   75% → CCR1 = 750
 *
 * ── TIM2 — 1 ms delay (no SysTick) ────────────────────────
 *  fTIM2_CLK = 90 MHz
 *  PSC = 89   → tick = 1 µs
 *  ARR = 999  → overflow every 1 ms
 *  ISR increments ms_count
 *
 * ── USART2 ─────────────────────────────────────────────────
 *  fAPB1 = 45 MHz
 *  USARTDIV = 45,000,000 / (16 × 115200) = 24.41
 *  Mantissa = 24, Fraction = 7  →  BRR = 0x187
 *
 * ── Sine LUT Breathing ─────────────────────────────────────
 *  LUT[i] = 50 × (1 + sin(2πi/256))   i = 0..255
 *  Range  : 0..100 (duty cycle %)
 *  Period : 256 × 8 ms = 2048 ms ≈ 2 s per breath
 * ============================================================
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

/* =========================================================
 * Global ms counter — incremented by TIM2 ISR
 * ========================================================= */
volatile uint32_t ms_count = 0;

/* =========================================================
 * Sine-wave LUT — 256 entries, values 0–100
 * LUT[i] = (uint8_t)(50.0 * (1.0 + sin(2π × i / 256)))
 * Pre-computed to avoid math at runtime
 * ========================================================= */
static const uint8_t sine_lut[256] = {
     50, 51, 52, 54, 55, 56, 57, 59, 60, 61, 62, 64, 65, 66, 67, 68,
     70, 71, 72, 73, 74, 75, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
     87, 88, 89, 89, 90, 91, 92, 93, 93, 94, 95, 95, 96, 97, 97, 98,
     98, 99, 99, 99,100,100,100,100,100,100,100,100,100,100, 99, 99,
     99, 98, 98, 97, 97, 96, 95, 95, 94, 93, 93, 92, 91, 90, 89, 89,
     88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 75, 74, 73, 72,
     71, 70, 68, 67, 66, 65, 64, 62, 61, 60, 59, 57, 56, 55, 54, 52,
     51, 50, 48, 47, 46, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32,
     31, 30, 28, 27, 26, 25, 24, 23, 21, 20, 19, 18, 17, 16, 15, 14,
     13, 12, 11, 10,  9,  9,  8,  7,  6,  6,  5,  4,  4,  3,  3,  2,
      2,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,
      1,  2,  2,  3,  3,  4,  4,  5,  6,  6,  7,  8,  9,  9, 10, 11,
     12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 23, 24, 25, 26, 27, 28,
     30, 31, 32, 33, 34, 36, 37, 38, 39, 41, 42, 43, 44, 46, 47, 48,
     49, 50, 51, 52, 53, 55, 56, 57, 58, 60, 61, 62, 63, 64, 66, 67,
     68, 69, 70, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 50
};

/* =========================================================
 * System Clock — 180 MHz
 * HSI(16) / PLLM(8) × PLLN(180) / PLLP(2) = 180 MHz
 * ========================================================= */
void SystemClock_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR      |= PWR_CR_VOS;              /* Voltage Scale 1 */

    /* Enable HSI */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    /* Flash: 5 wait states @ 180 MHz / 3.3V */
    FLASH->ACR  = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |=  FLASH_ACR_LATENCY_5WS;

    /* PLL: 16/8=2 MHz × 180 = 360 MHz VCO / 2 = 180 MHz */
    RCC->PLLCFGR  = 0;
    RCC->PLLCFGR |= (8U   << RCC_PLLCFGR_PLLM_Pos);
    RCC->PLLCFGR |= (180U << RCC_PLLCFGR_PLLN_Pos);
    RCC->PLLCFGR |= (0U   << RCC_PLLCFGR_PLLP_Pos);  /* 00 = /2 */
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI;
    RCC->PLLCFGR |= (2U   << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* OverDrive required for 180 MHz on F446 */
    PWR->CR |= PWR_CR_ODEN;
    while (!(PWR->CSR & PWR_CSR_ODRDY));
    PWR->CR |= PWR_CR_ODSWEN;
    while (!(PWR->CSR & PWR_CSR_ODSWRDY));

    /* Bus prescalers — set BEFORE switching clock */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;    /* AHB  = 180 MHz */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;   /* APB1 =  45 MHz */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;   /* APB2 =  90 MHz */

    /* Switch to PLL */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* =========================================================
 * USART2 — 115200 8N1
 * PA2 = TX (AF7), PA3 = RX (AF7)
 * fAPB1 = 45 MHz → BRR = 0x187
 * ========================================================= */
static void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 → AF7 TX */
    GPIOA->MODER  &= ~(3U << (2*2));
    GPIOA->MODER  |=  (2U << (2*2));
    GPIOA->AFR[0] &= ~(0xFU << (4*2));
    GPIOA->AFR[0] |=  (7U   << (4*2));

    /* PA3 → AF7 RX */
    GPIOA->MODER  &= ~(3U << (3*2));
    GPIOA->MODER  |=  (2U << (3*2));
    GPIOA->AFR[0] &= ~(0xFU << (4*3));
    GPIOA->AFR[0] |=  (7U   << (4*3));

    USART2->BRR = (24U << 4) | 7U;   /* 0x187 → 115200 baud */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void USART2_SendString(const char *s)
{
    while (*s) {
        while (!(USART2->SR & USART_SR_TXE)) {}
        USART2->DR = (uint8_t)(*s++);
    }
    while (!(USART2->SR & USART_SR_TC)) {}
}

/* =========================================================
 * TIM2 — 1 ms interrupt for delay_ms()
 *
 * fTIM2_CLK = 90 MHz
 * PSC = 89  → tick = 1 µs
 * ARR = 999 → overflow every 1000 µs = 1 ms
 * ========================================================= */
static void TIM2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    __NOP(); __NOP();

    TIM2->CR1  &= ~TIM_CR1_CEN;
    TIM2->PSC   = 89U;
    TIM2->ARR   = 999U;
    TIM2->EGR   = TIM_EGR_UG;
    TIM2->SR    = 0U;
    TIM2->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, 0U);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;
        ms_count++;
    }
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = ms_count;
    while ((ms_count - start) < ms) {}
}

/* =========================================================
 * PA5 — LD2 onboard LED (timing reference)
 * ========================================================= */
static void LD2_Init(void)
{
    /* GPIOA already enabled by USART2_Init */
    GPIOA->MODER  &= ~(3U << (5*2));
    GPIOA->MODER  |=  (1U << (5*2));   /* Output mode */
    GPIOA->OTYPER &= ~(1U << 5);       /* Push-pull   */
    GPIOA->OSPEEDR|=  (3U << (5*2));   /* High speed  */
    GPIOA->PUPDR  &= ~(3U << (5*2));   /* No pull     */
}

static inline void LD2_Toggle(void) { GPIOA->ODR ^= (1U << 5); }
static inline void LD2_On(void)     { GPIOA->BSRR = (1U << 5); }
static inline void LD2_Off(void)    { GPIOA->BSRR = (1U << (5+16)); }

/* =========================================================
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
 * ========================================================= */
#define TIM3_ARR  999U    /* ARR value — defines PWM period */

static void TIM3_PWM_Init(void)
{
    /* 1. Enable clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    __NOP(); __NOP();

    /* 2. PA6 → Alternate Function mode */
    GPIOA->MODER   &= ~(3U << (6*2));
    GPIOA->MODER   |=  (2U << (6*2));    /* AF mode          */

    /* 3. PA6 push-pull, very high speed, no pull */
    GPIOA->OTYPER  &= ~(1U << 6);        /* Push-pull        */
    GPIOA->OSPEEDR |=  (3U << (6*2));    /* Very high speed  */
    GPIOA->PUPDR   &= ~(3U << (6*2));    /* No pull          */

    /* 4. PA6 → AF2 (TIM3_CH1)
     *    AFR[0] covers pins 0-7, pin 6 → bits [27:24]       */
    GPIOA->AFR[0] &= ~(0xFU << (4*6));
    GPIOA->AFR[0] |=  (2U   << (4*6));   /* AF2 = TIM3       */

    /* 5. TIM3 base config */
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->PSC  = 89U;                    /* tick = 1 µs      */
    TIM3->ARR  = TIM3_ARR;               /* period = 1000 µs = 1 ms → 1 kHz */

    /* 6. PWM Mode 1 on Channel 1
     *    OC1M = 110 in CCMR1 [6:4]
     *    OC1PE = 1 → preload enable (glitch-free updates)   */
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |=  TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;  /* 110 */
    TIM3->CCMR1 |=  TIM_CCMR1_OC1PE;

    /* 7. Default CCR1 = 0 (0% duty) */
    TIM3->CCR1 = 0U;

    /* 8. Enable Channel 1 output, active high (CC1P = 0) */
    TIM3->CCER |=  TIM_CCER_CC1E;
    TIM3->CCER &= ~TIM_CCER_CC1P;

    /* 9. Auto-reload preload enable */
    TIM3->CR1 |= TIM_CR1_ARPE;

    /* 10. Force update event → load PSC/ARR/CCR1 shadow regs */
    TIM3->EGR  = TIM_EGR_UG;
    TIM3->SR   = 0U;

    /* 11. Start counter */
    TIM3->CR1 |= TIM_CR1_CEN;
}

/* =========================================================
 * PWM_SetDuty — Algorithm A4.2
 *
 * Accepts pct = 0..100
 * Computes CCR1 = pct × (ARR+1) / 100
 *               = pct × 1000    / 100
 *               = pct × 10
 *
 * Because OC1PE is set, the new CCR1 takes effect at the
 * next Update Event → glitch-free transitions.
 * ========================================================= */
static void PWM_SetDuty(uint8_t pct)
{
    if (pct > 100U) pct = 100U;           /* clamp */
    TIM3->CCR1 = (uint32_t)pct * (TIM3_ARR + 1U) / 100U;
}

/* =========================================================
 * Main
 * ========================================================= */
int main(void)
{
    char buf[64];

    SystemClock_Config();
    USART2_Init();
    TIM2_Init();        /* ms_count via TIM2 NVIC — no SysTick */
    LD2_Init();
    TIM3_PWM_Init();    /* PA6 PWM @ 1 kHz                     */

    /* ─────────────────────────────────────────────────────
     * Header
     * ───────────────────────────────────────────────────── */
    USART2_SendString("\r\n======================================\r\n");
    USART2_SendString("  Task 4: TIM3 PWM — STM32F446RE\r\n");
    USART2_SendString("  PA6, 1 kHz, PSC=89, ARR=999\r\n");
    USART2_SendString("======================================\r\n");

    /* ─────────────────────────────────────────────────────
     * Part 1 — Duty-cycle sweep: 0% to 100% in 10% steps
     * Hold each step for 300 ms
     * Print: Duty = XX%   CCR1 = NNN
     * ───────────────────────────────────────────────────── */
    USART2_SendString("\r\n-- Part 1: Duty-Cycle Sweep --\r\n");
    USART2_SendString("Duty    | CCR1\r\n");
    USART2_SendString("--------|------\r\n");

    for (uint8_t pct = 0; pct <= 100; pct += 10)
    {
        PWM_SetDuty(pct);
        LD2_Toggle();   /* toggle LD2 as timing reference */

        uint32_t ccr1 = (uint32_t)pct * (TIM3_ARR + 1U) / 100U;
        snprintf(buf, sizeof(buf),
                 "Duty = %3u%%  |  CCR1 = %4lu\r\n",
                 pct, (unsigned long)ccr1);
        USART2_SendString(buf);

        delay_ms(300U);
    }

    /* ─────────────────────────────────────────────────────
     * Part 2 — Sine-wave breathing effect (5 cycles)
     * LUT[i] = 50 × (1 + sin(2πi/256))
     * Each step holds 8 ms → full period = 256×8 = 2048 ms
     * ───────────────────────────────────────────────────── */
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

    /* ─────────────────────────────────────────────────────
     * Part 3 — Final hold at exactly 50%
     * ───────────────────────────────────────────────────── */
    PWM_SetDuty(50U);
    uint32_t final_ccr1 = (uint32_t)50U * (TIM3_ARR + 1U) / 100U;
    snprintf(buf, sizeof(buf),
             "\r\n-- Final Hold --\r\nDuty = 50%%   CCR1 = %lu\r\n",
             (unsigned long)final_ccr1);
    USART2_SendString(buf);

    LD2_On();   /* LD2 stays ON to indicate completion */

    USART2_SendString("\r\n====== Task 4 Complete ======\r\n");

    while (1) {}
}

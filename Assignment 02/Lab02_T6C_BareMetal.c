/*
 * ============================================================
 * CSE 2206 — Microcontroller & Embedded System Lab-02
 * Task 6:
 * Option C: Input Capture Frequency/Duty Measurement (TIM5, PA0)
 *
 * Platform : STM32F446RE Nucleo-64
 * fTIM4_CLK = fTIM2_CLK = fTIM5_CLK = 90 MHz (APB1 × 2)
 * ============================================================
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void SystemClock_Config(void) {
	/* 1. Enable PWR clock */
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;

	/* 2. Voltage scaling (Scale 1 mode) */
	PWR->CR |= PWR_CR_VOS;

	/* 3. Enable HSI */
	RCC->CR |= RCC_CR_HSION;
	while (!(RCC->CR & RCC_CR_HSIRDY));

	/* 4. Configure FLASH latency and enable caches */
	FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_5WS;

	/* 5. Configure PLL
	 HSI = 16 MHz
	 PLLM = 8
	 PLLN = 180
	 PLLP = 2
	 PLLQ = 2
	 */

	RCC->PLLCFGR = 0;
	RCC->PLLCFGR |= (8 << RCC_PLLCFGR_PLLM_Pos);
	RCC->PLLCFGR |= (180 << RCC_PLLCFGR_PLLN_Pos);
	RCC->PLLCFGR |= (0 << RCC_PLLCFGR_PLLP_Pos);   // PLLP = 2
	RCC->PLLCFGR |= (RCC_PLLCFGR_PLLSRC_HSI);
	RCC->PLLCFGR |= (2 << RCC_PLLCFGR_PLLQ_Pos);

	/* 6. Enable PLL */
	RCC->CR |= RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY));

	/* 7. Enable OverDrive mode */
	PWR->CR |= PWR_CR_ODEN;
	while (!(PWR->CSR & PWR_CSR_ODRDY));

	PWR->CR |= PWR_CR_ODSWEN;
	while (!(PWR->CSR & PWR_CSR_ODSWRDY));

	/* 8. Configure Bus Prescalers
	 AHB = SYSCLK /1
	 APB1 = HCLK /4
	 APB2 = HCLK /2
	 */

	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;

	/* 9. Select PLL as system clock */
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* =========================================================
 * SECTION 1 — USART2
 * PA2=TX, PA3=RX, 115200 8N1, APB1 clock = 45 MHz
 * ========================================================= */

/**
 * @brief  Initialise USART2 for 115200 8N1.
 *         PA2 → TX (AF7), PA3 → RX (AF7).
 */
static void USART2_Init(void)
{
    /* 1. Enable clocks */
    RCC->AHB1ENR  |= RCC_AHB1ENR_GPIOAEN;   /* GPIOA clock */
    RCC->APB1ENR  |= RCC_APB1ENR_USART2EN;  /* USART2 clock */

    /* 2. PA2: AF7 (USART2_TX) */
    GPIOA->MODER  &= ~(3U << (2*2));
    GPIOA->MODER  |=  (2U << (2*2));         /* Alternate function */
    GPIOA->AFR[0] &= ~(0xF << (4*2));
    GPIOA->AFR[0] |=  (7U  << (4*2));        /* AF7 = USART2 */

    /* 3. PA3: AF7 (USART2_RX) */
    GPIOA->MODER  &= ~(3U << (3*2));
    GPIOA->MODER  |=  (2U << (3*2));
    GPIOA->AFR[0] &= ~(0xF << (4*3));
    GPIOA->AFR[0] |=  (7U  << (4*3));

	/* 3. Baud rate calculation
	   fCK = 45 MHz
	   Baud = 115200
	   USARTDIV = 24.414 (fCK/(16*Baud))
	   Mantissa = 24
	   Fraction ≈ 7
	*/

	USART2->BRR = (24 << 4) | 7;   // 0x187

    /* 5. Enable TX, RX, and the peripheral */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/**
 * @brief  Transmit a null-terminated string over USART2.
 * @param  s  Pointer to string (must be null-terminated).
 */
static void USART2_SendString(const char *s)
{
    while (*s)
    {
        while (!(USART2->SR & USART_SR_TXE)) {}   /* Wait TX empty */
        USART2->DR = (uint8_t)(*s++);
    }
    while (!(USART2->SR & USART_SR_TC)) {}         /* Wait TX complete */
}

/* =========================================================
 * SECTION 2 — TIM6 Initialisation for 1 µs tick
 *
 * fTIM6_CLK = 90 MHz
 * PSC = 89  → tick frequency = 90e6 / (89+1) = 1 MHz (1 µs per tick)
 * ARR = 0xFFFF (maximum 16-bit range = 65535 µs per single overflow)
 * ========================================================= */

/**
 * @brief  Initialise TIM6 with 1 µs tick for delay functions.
 *         TIM6 is on APB1 → TIM_CLK = 2 × APB1 = 90 MHz.
 */
static void TIM6_Init(void)
{
    /* Step 1: Enable TIM6 peripheral clock via RCC APB1 */
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    /* Step 2: Short NOP delay for clock stabilisation */
    __NOP(); __NOP(); __NOP(); __NOP();

    /* Step 3: Disable counter before configuring */
    TIM6->CR1 &= ~TIM_CR1_CEN;

    /* Step 4: Prescaler — 1 µs resolution
     *   tick = fTIM6_CLK / (PSC+1) = 90 MHz / 90 = 1 MHz → 1 µs  */
    TIM6->PSC = 89U;

    /* Step 5: Auto-reload = max 16-bit value (65535 µs = ~65.5 ms max) */
    TIM6->ARR = 0xFFFFU;

    /* Step 6: Force immediate register update (shadow registers loaded) */
    TIM6->EGR = TIM_EGR_UG;

    /* Step 7: Clear all status flags */
    TIM6->SR = 0U;

    /* Step 8: Start the counter */
    TIM6->CR1 |= TIM_CR1_CEN;
}

/* =========================================================
 * SECTION 3 — Delay Functions
 * ========================================================= */

/**
 * @brief  Microsecond blocking delay using TIM6.
 * @param  us  Delay in microseconds (max 65535 due to 16-bit counter).
 *
 * NOTE: If an ISR fires during this delay and takes N µs,
 *       the delay is extended by N µs. For ISR-safe operation,
 *       capture an absolute timestamp before the loop and compare
 *       against (start + us) with rollover handling.
 */
static void delay_us(uint16_t us)
{
    /* Reset counter to zero */
    TIM6->CNT = 0U;

    /* Busy-wait until CNT reaches requested µs count.
     * Cast to uint16_t ensures correct comparison within 16-bit range. */
    while ((uint16_t)TIM6->CNT < us) {}
}

/**
 * @brief  Millisecond blocking delay.
 * @param  ms  Delay in milliseconds (32-bit, no practical upper limit).
 *
 * Why loop around delay_us(1000) rather than delay_us(ms*1000)?
 *   → ms*1000 can overflow uint16_t for ms > 65 (e.g., 100 ms → 100000,
 *     which wraps to ~34464). Looping is safe for any ms value.
 */
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        delay_us(1000U);   /* 1 ms = 1000 µs */
    }
}


/*
 * ══════════════════════════════════════════════════════════
 *  OPTION C — INPUT CAPTURE: MEASURING TIM3_CH1 ON TIM5_CH1
 * ══════════════════════════════════════════════════════════
 *
 * Connection: PA6 (TIM3_CH1 output) → PA0 (TIM5_CH1 input)
 *   via jumper wire on the breadboard.
 *
 * fTIM5 = 90 MHz, PSC=89 → 1 µs tick
 *
 * Dual-edge capture on TI1:
 *   TIM5_CH1 → rising edge (trise)
 *   TIM5_CH2 → falling edge via IC2→TI1 mapping (tfall)
 *
 * Period  = trise2 - trise1  (in µs)
 * HighTime= tfall  - trise1  (in µs)
 * Freq    = 1 000 000 / Period
 * Duty    = HighTime × 100 / Period
 */


/* =========================================================
 * PWM CONFIGURATION CONSTANTS
 *
 * Goal: 1 kHz PWM on TIM3_CH1
 * fTIM3 = 90 MHz, PSC = 89 → timer tick = 1 MHz
 * ARR = 999 → period = 1000 ticks = 1 ms → f = 1 kHz
 *
 * CCR1 formula: CCR1 = duty_percent × (ARR+1) / 100
 * ========================================================= */
#define TIM3_PWM_PSC   89U     /* Prescaler: 90 MHz / 90 = 1 MHz tick */
#define TIM3_PWM_ARR   999U    /* Auto-reload: 1 kHz period            */

/**
 * @brief  Configure TIM3 Channel 1 for 1 kHz PWM on PA6 (AF2).
 *
 * Register steps follow Algorithm A4.1 from the assignment.
 */
static void TIM3_PWM_Init(void)
{
    /* Step 1: Enable peripheral clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;   /* GPIOA */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;    /* TIM3  */
    __NOP(); __NOP();

    /* Step 2: Configure PA6 as Alternate Function, Push-Pull,
     *         Very High Speed, No Pull-up/Pull-down            */
    GPIOA->MODER  &= ~(3U << (6*2));
    GPIOA->MODER  |=  (2U << (6*2));        /* MODER = 10 (Alternate function) */
    GPIOA->OTYPER &= ~(1U << 6);            /* OTYPER = 0 (Push-Pull)          */
    GPIOA->OSPEEDR|=  (3U << (6*2));        /* OSPEEDR = 11 (Very High Speed)  */
    GPIOA->PUPDR  &= ~(3U << (6*2));        /* PUPDR = 00 (No pull)            */

    /* Step 3: Set Alternate Function for PA6 → AF2 (TIM3_CH1)
     *         AFR[0] covers pins 0–7; pin 6 occupies bits [27:24]   */
    GPIOA->AFR[0] &= ~(0xFU << (4*6));
    GPIOA->AFR[0] |=  (0x2U << (4*6));      /* AF2 = TIM3             */

    /* Step 4: Disable TIM3 counter before configuration */
    TIM3->CR1 &= ~TIM_CR1_CEN;

    /* Step 5: PSC and ARR for 1 kHz */
    TIM3->PSC = TIM3_PWM_PSC;
    TIM3->ARR = TIM3_PWM_ARR;

    /* Step 6a: Configure CH1 output compare — PWM Mode 1 (OC1M = 110b)
     *          CCMR1 bits [6:4] = OC1M, clear then set                   */
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |=  (6U << TIM_CCMR1_OC1M_Pos);  /* PWM Mode 1: 0b110 */

    /* Step 6b: Enable Output Compare 1 Preload (OC1PE) — glitch-free updates */
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE;

    /* Step 7: Enable CH1 output (CC1E), active-high polarity (CC1P = 0) */
    TIM3->CCER |= TIM_CCER_CC1E;
    TIM3->CCER &= ~TIM_CCER_CC1P;

    /* Step 8: Enable Auto-Reload Preload (ARPE) */
    TIM3->CR1 |= TIM_CR1_ARPE;

    /* Step 9: Force Update Event to load PSC/ARR shadow registers */
    TIM3->EGR = TIM_EGR_UG;

    /* Step 10: Start counter */
    TIM3->CR1 |= TIM_CR1_CEN;
}

/* =========================================================
 * SECTION 2 — Duty Cycle Control (Algorithm A4.2)
 * ========================================================= */

/**
 * @brief  Set TIM3 CH1 PWM duty cycle at runtime.
 * @param  percent  Duty cycle 0–100 (clamped if > 100).
 *
 * CCR1 is written while preload is enabled → takes effect
 * at the next Update Event → glitch-free transition.
 */
static void PWM_SetDuty(uint8_t percent)
{
    if (percent > 100U) percent = 100U;

    /* CCR1 = duty% × (ARR+1) / 100
     * Use 32-bit multiply to prevent intermediate overflow. */
    uint32_t ccr = (uint32_t)percent * (TIM3_PWM_ARR + 1U) / 100U;
    TIM3->CCR1 = ccr;
}

/**
 * @brief  Initialise TIM5 for input capture (rising and falling edges on PA0).
 *         PSC=89 → 1 µs tick for easy time reading.
 */
static void TIM5_InputCapture_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    __NOP(); __NOP();

    /* PA0: AF2 (TIM5_CH1), Input (MODER=00), No pull */
    GPIOA->MODER  &= ~(3U << (0*2));    /* Input mode — MODER=00 is reset default */
    GPIOA->PUPDR  &= ~(3U << (0*2));    /* No pull */
    GPIOA->AFR[0] &= ~(0xFU << (4*0));
    GPIOA->AFR[0] |=  (0x2U << (4*0)); /* AF2 = TIM5 */
    /* Re-set to AF mode */
    GPIOA->MODER  |=  (2U << (0*2));

    TIM5->CR1 &= ~TIM_CR1_CEN;

    /* PSC = 89 → 1 µs resolution */
    TIM5->PSC = 89U;
    TIM5->ARR = 0xFFFFFFFFU;   /* Max 32-bit, free running */

    /* ── CCMR1: Configure capture channels ──
     * CH1 (CC1): Input Capture on TI1, rising edge
     *   CC1S = 01 (IC1 mapped to TI1)
     *   IC1F = 0000 (no filter)
     *   IC1PSC = 00 (no prescaler)
     *
     * CH2 (CC2): Input Capture on TI1 (indirect — falling edge)
     *   CC2S = 10 (IC2 mapped to TI1 input)
     *   IC2F = 0000, IC2PSC = 00
     */
    TIM5->CCMR1 = 0U;
    TIM5->CCMR1 |= (1U << TIM_CCMR1_CC1S_Pos);    /* CC1S = 01 → TI1 */
    TIM5->CCMR1 |= (2U << TIM_CCMR1_CC2S_Pos);    /* CC2S = 10 → TI1 (indirect) */

    /* ── CCER: CH1 rising, CH2 falling ──
     * CC1P = 0, CC1NP = 0 → rising edge
     * CC2P = 1, CC2NP = 0 → falling edge                         */
    TIM5->CCER = 0U;
    TIM5->CCER |= TIM_CCER_CC1E;                   /* Enable CH1 capture */
    TIM5->CCER |= TIM_CCER_CC2E | TIM_CCER_CC2P;  /* Enable CH2, falling edge */

    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR  = 0U;
    TIM5->CR1 |= TIM_CR1_CEN;
}

/**
 * @brief  Capture one period and duty cycle measurement.
 * @param[out] freq_hz   Measured frequency in Hz.
 * @param[out] duty_pct  Measured duty cycle 0–100.
 */
static void TIM5_Measure(uint32_t *freq_hz, uint32_t *duty_pct)
{
    uint32_t trise1, tfall, trise2;
    uint32_t period_us, high_us;
    uint32_t timeout;

    TIM5->SR = 0U;

    /* Wait for CC1IF (rising edge) */
    timeout = 1000000U;
    while (!(TIM5->SR & TIM_SR_CC1IF)) {
        if (--timeout == 0) { USART2_SendString("HUNG: waiting for CC1IF (rise1)\r\n"); return; }
    }
    trise1 = TIM5->CCR1;
    TIM5->SR &= ~TIM_SR_CC1IF;
    USART2_SendString("Got rise1\r\n");

    /* Wait for CC2IF (falling edge) */
    timeout = 1000000U;
    while (!(TIM5->SR & TIM_SR_CC2IF)) {
        if (--timeout == 0) { USART2_SendString("HUNG: waiting for CC2IF (fall)\r\n"); return; }
    }
    tfall = TIM5->CCR2;
    TIM5->SR &= ~TIM_SR_CC2IF;
    USART2_SendString("Got fall\r\n");

    /* Wait for second CC1IF (rising edge) */
    timeout = 1000000U;
    while (!(TIM5->SR & TIM_SR_CC1IF)) {
        if (--timeout == 0) { USART2_SendString("HUNG: waiting for CC1IF (rise2)\r\n"); return; }
    }
    trise2 = TIM5->CCR1;
    TIM5->SR &= ~TIM_SR_CC1IF;
    USART2_SendString("Got rise2\r\n");

    period_us = trise2 - trise1;
    high_us   = tfall  - trise1;
    if (high_us > period_us) high_us = period_us;

    *freq_hz  = (period_us > 0U) ? (1000000U / period_us) : 0U;
    *duty_pct = (period_us > 0U) ? (high_us * 100U / period_us) : 0U;
}

int main(void)
{
    char buf[120];

    SystemClock_Config();
    USART2_Init();
    TIM6_Init();
    TIM3_PWM_Init();    /* Generate 1 kHz signal on PA6 */
    TIM5_InputCapture_Init();

    USART2_SendString("\r\n===== Lab-02 Task 6C: Input Capture (Bare-Metal) =====\r\n");
    USART2_SendString("Duty Set | Freq (Hz) | Duty Meas | Period (us) | High (us)\r\n");
    USART2_SendString("---------+-----------+-----------+-------------+----------\r\n");

    /* Measure at several duty cycles to verify accuracy */
    uint8_t duties[] = {25U, 50U, 75U};
    for (int i = 0; i < 3; i++)
    {
        PWM_SetDuty(duties[i]);
        delay_ms(5U);   /* Let PWM stabilise */

        uint32_t freq = 0, duty = 0;
        TIM5_Measure(&freq, &duty);

        snprintf(buf, sizeof(buf),
            "%7u%% | %9lu | %9lu%% | TIM3=1kHz config\r\n",
            duties[i], (unsigned long)freq, (unsigned long)duty);
        USART2_SendString(buf);
    }

    USART2_SendString("===== Task 6C Complete =====\r\n");
    while (1) {}
}

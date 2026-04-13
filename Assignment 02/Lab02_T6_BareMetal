/*
 * ============================================================
 * CSE 2206 — Microcontroller & Embedded System Lab-02
 * Task 6 Option A — Passive Buzzer Tone Generation (TIM4)
 * Platform : STM32F446RE Nucleo-64
 * Output   : PB6 → TIM4 Channel 1 (AF2) → Passive Buzzer
 *
 * ── TIM4 PWM Calculations ──────────────────────────────────
 *  fTIM4_CLK = 90 MHz
 *    (APB1 bus = 45 MHz, prescaler≠1 → timer clock = 45×2 = 90 MHz)
 *
 *  PSC = 99
 *  tick_freq = fTIM4_CLK / (PSC+1)
 *            = 90,000,000 / 100
 *            = 900,000 Hz  →  900 kHz
 *
 *  ARR formula (from desired frequency):
 *    f_out = tick_freq / (ARR+1)
 *    ARR   = (tick_freq / f_out) - 1
 *          = (900,000  / f_out) - 1
 *
 *  Example — C4 (261.6 Hz):
 *    ARR = (900,000 / 261.6) - 1
 *        = 3440.4 - 1
 *        ≈ 3439  (rounded, table uses 3436 — slight tuning)
 *
 *  50% duty cycle:
 *    CCR1 = (ARR + 1) / 2
 *    When CNT < CCR1 → output HIGH
 *    When CNT ≥ CCR1 → output LOW
 *    → equal on/off time = 50%
 *
 *  Back-calculated actual frequency from ARR:
 *    f_actual = 900,000 / (ARR + 1)
 *
 * ── TIM2 + NVIC for delay_ms ───────────────────────────────
 *  Same TIM2 setup from Task 3 (ms_count via interrupt).
 *  delay_ms() simply waits until ms_count advances by N.
 * ============================================================
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>

/* =========================================================
 * Global millisecond counter — incremented by TIM2 ISR
 * ========================================================= */
volatile uint32_t ms_count = 0;

/* =========================================================
 * System Clock — fCPU = 180 MHz
 * HSI(16) / PLLM(8) × PLLN(180) / PLLP(2) = 180 MHz
 * Prescalers set BEFORE PLL switch to protect APB1 (max 45 MHz)
 * ========================================================= */
void SystemClock_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR      |= PWR_CR_VOS;              /* Scale 1 for 180 MHz  */

    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    /* Flash: 5 wait states required at 180 MHz / 3.3V */
    FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |=  FLASH_ACR_LATENCY_5WS;

    /* PLL: 16/8=2 MHz input × 180 = 360 MHz VCO / 2 = 180 MHz */
    RCC->PLLCFGR  = 0;
    RCC->PLLCFGR |= (8U   << RCC_PLLCFGR_PLLM_Pos);
    RCC->PLLCFGR |= (180U << RCC_PLLCFGR_PLLN_Pos);
    RCC->PLLCFGR |= (0U   << RCC_PLLCFGR_PLLP_Pos); /* 00 = /2 */
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI;
    RCC->PLLCFGR |= (2U   << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* OverDrive mandatory for 180 MHz */
    PWR->CR |= PWR_CR_ODEN;
    while (!(PWR->CSR & PWR_CSR_ODRDY));
    PWR->CR |= PWR_CR_ODSWEN;
    while (!(PWR->CSR & PWR_CSR_ODSWRDY));

    /* Bus prescalers FIRST, then switch */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;    /* AHB  = 180 MHz */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;   /* APB1 =  45 MHz */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;   /* APB2 =  90 MHz */

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* =========================================================
 * USART2 — 115200 8N1, PA2=TX, PA3=RX (AF7)
 * fCK=45MHz → USARTDIV=24.41 → Mantissa=24, Frac=7 → BRR=0x187
 * ========================================================= */
static void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER  &= ~(3U << (2*2));
    GPIOA->MODER  |=  (2U << (2*2));
    GPIOA->AFR[0] &= ~(0xFU << (4*2));
    GPIOA->AFR[0] |=  (7U   << (4*2));   /* PA2 → AF7 TX */

    GPIOA->MODER  &= ~(3U << (3*2));
    GPIOA->MODER  |=  (2U << (3*2));
    GPIOA->AFR[0] &= ~(0xFU << (4*3));
    GPIOA->AFR[0] |=  (7U   << (4*3));   /* PA3 → AF7 RX */

    USART2->BRR = (24U << 4) | 7U;
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
 * ISR increments ms_count every overflow
 * ========================================================= */
static void TIM2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    __NOP(); __NOP();

    TIM2->CR1  &= ~TIM_CR1_CEN;
    TIM2->PSC   = 89U;     /* 1 µs per tick                     */
    TIM2->ARR   = 999U;    /* overflow every 1000 ticks = 1 ms  */
    TIM2->EGR   = TIM_EGR_UG;
    TIM2->SR    = 0U;
    TIM2->DIER |= TIM_DIER_UIE;  /* enable update interrupt      */

    NVIC_SetPriority(TIM2_IRQn, 0U);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1  |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;   /* clear flag first */
        ms_count++;
    }
}

/* =========================================================
 * delay_ms — uses ms_count from TIM2 ISR
 *
 * Waits until ms_count has advanced by 'ms' counts.
 * No busy-loop on hardware register — purely counter-based.
 * ========================================================= */
static void delay_ms(uint32_t ms)
{
    uint32_t start = ms_count;
    while ((ms_count - start) < ms) {}
}

/* =========================================================
 * TIM4 PWM — Channel 1 on PB6 (AF2)
 *
 * ── Hardware path ──────────────────────────────────────
 *  TIM4_CH1 alternate function = AF2
 *  PB6 must be set to AF mode with AFR = 2
 *
 * ── PWM Mode 1 (OC1M = 110) ───────────────────────────
 *  CNT < CCR1 → PB6 HIGH
 *  CNT ≥ CCR1 → PB6 LOW
 *  CCR1 = (ARR+1)/2 → 50% duty cycle
 *
 * ── Init sets up the peripheral only ──────────────────
 *  Actual ARR/CCR1 values are set per-note in Buzzer_PlayNote()
 * ========================================================= */
static void TIM4_PWM_Init(void)
{
    /* 1. Enable clocks for GPIOB and TIM4 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    __NOP(); __NOP();

    /* 2. PB6 → Alternate Function mode */
    GPIOB->MODER  &= ~(3U << (6*2));
    GPIOB->MODER  |=  (2U << (6*2));   /* AF mode            */

    /* 3. PB6 → AF2 (TIM4_CH1)
     *    AFR[0] covers pins 0–7, pin 6 → bits [27:24]        */
    GPIOB->AFR[0] &= ~(0xFU << (4*6));
    GPIOB->AFR[0] |=  (2U   << (4*6)); /* AF2 = TIM4          */

    /* 4. TIM4 base configuration
     *    PSC=99: tick_freq = 90MHz/100 = 900kHz (1.11 µs/tick)
     *    ARR placeholder — overwritten per note               */
    TIM4->CR1  &= ~TIM_CR1_CEN;
    TIM4->PSC   = 99U;
    TIM4->ARR   = 3436U;               /* C4 default          */

    /* 5. PWM Mode 1 on Channel 1
     *    OC1M = 110 in CCMR1[6:4]
     *    OC1PE = 1  → preload enable (ARR updated smoothly)   */
    TIM4->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM4->CCMR1 |=  TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; /* 110 */
    TIM4->CCMR1 |=  TIM_CCMR1_OC1PE;

    /* 6. CCR1 = (ARR+1)/2 for 50% duty */
    TIM4->CCR1  = (TIM4->ARR + 1U) / 2U;

    /* 7. Enable Channel 1 output (active high) */
    TIM4->CCER  |= TIM_CCER_CC1E;

    /* 8. Auto-reload preload enable */
    TIM4->CR1   |= TIM_CR1_ARPE;

    /* 9. Force register update and start */
    TIM4->EGR    = TIM_EGR_UG;
    TIM4->SR     = 0U;
    TIM4->CR1   |= TIM_CR1_CEN;
}

/* =========================================================
 * Buzzer_PlayNote
 *
 * Changes TIM4 frequency to produce a musical note.
 *
 * Algorithm (from lab sheet A6.A):
 *  1. Compute ARR for target freq: ARR = (900000/freq) - 1
 *     (we receive pre-computed ARR from the note table)
 *  2. Stop TIM4 counter
 *  3. Update ARR and CCR1 = (ARR+1)/2 for 50% duty
 *  4. Force Update Event (EGR UG=1) to load shadow registers
 *  5. Restart counter
 *  6. Hold for duration_ms using delay_ms()
 *
 * Back-calculated actual frequency printed over UART:
 *   f_actual = 900,000 / (ARR + 1)
 *   printed as integer.fraction using % arithmetic
 *
 * Parameters:
 *   name        — note name string e.g. "C4"
 *   arr         — pre-computed ARR value from table
 *   duration_ms — how long to hold the note
 * ========================================================= */
static void Buzzer_PlayNote(const char *name, uint32_t arr,
                            uint32_t duration_ms)
{
    char buf[96];

    /* Step 2: Stop counter before changing ARR */
    TIM4->CR1 &= ~TIM_CR1_CEN;

    /* Step 3: Load new ARR and CCR1
     *   CCR1 = (ARR+1)/2 → 50% duty cycle
     *   If ARR=3436 → CCR1=1718 → HIGH for 1718 ticks, LOW for 1719 */
    TIM4->ARR  = arr;
    TIM4->CCR1 = (arr + 1U) / 2U;

    /* Step 4: Force update event — loads PSC/ARR/CCR1 shadow regs */
    TIM4->EGR  = TIM_EGR_UG;
    TIM4->SR   = 0U;                   /* clear UIF set by UG      */

    /* Step 5: Restart counter */
    TIM4->CR1 |= TIM_CR1_CEN;

    /* Back-calculate actual frequency from ARR:
     *   f_actual = tick_freq / (ARR+1)
     *            = 900,000  / (ARR+1)
     *
     *   Integer split for UART (no float):
     *   f_whole = 900000 / (ARR+1)
     *   f_frac  = (900000 * 10 / (ARR+1)) % 10  → 1 decimal place */
    uint32_t f_whole = 900000U / (arr + 1U);
    uint32_t f_frac  = (900000U * 10U / (arr + 1U)) % 10U;

    snprintf(buf, sizeof(buf),
        "Note: %-3s | ARR: %4lu | CCR1: %4lu | f_actual: %lu.%lu Hz\r\n",
        name,
        (unsigned long)arr,
        (unsigned long)((arr + 1U) / 2U),
        (unsigned long)f_whole,
        (unsigned long)f_frac);
    USART2_SendString(buf);

    /* Step 6: Hold note for requested duration */
    delay_ms(duration_ms);
}

/* =========================================================
 * Buzzer_Silence
 * Stops TIM4 output so there is a clean gap between notes.
 * ========================================================= */
static void Buzzer_Silence(uint32_t duration_ms)
{
    TIM4->CR1 &= ~TIM_CR1_CEN;   /* stop PWM → pin goes LOW */
    delay_ms(duration_ms);
}

/* =========================================================
 * Note table — C-major scale
 *
 * ARR = (900,000 / freq) - 1
 *
 * C4 (261.6 Hz): 900000/261.6 - 1 = 3440 - 1 ≈ 3436 (table value)
 * D4 (293.7 Hz): 900000/293.7 - 1 = 3065 - 1 ≈ 3061
 * E4 (329.6 Hz): 900000/329.6 - 1 = 2731 - 1 ≈ 2727
 * F4 (349.2 Hz): 900000/349.2 - 1 = 2578 - 1 ≈ 2572
 * G4 (392.0 Hz): 900000/392.0 - 1 = 2296 - 1 ≈ 2295
 * A4 (440.0 Hz): 900000/440.0 - 1 = 2045 - 1 ≈ 2044
 * B4 (493.9 Hz): 900000/493.9 - 1 = 1822 - 1 ≈ 1821
 * C5 (523.3 Hz): 900000/523.3 - 1 = 1720 - 1 ≈ 1717
 * ========================================================= */
typedef struct {
    const char *name;
    uint32_t    arr;
} Note_t;

/* C-major scale */
static const Note_t scale[] = {
    { "C4", 3436U },
    { "D4", 3061U },
    { "E4", 2727U },
    { "F4", 2572U },
    { "G4", 2295U },
    { "A4", 2044U },
    { "B4", 1821U },
    { "C5", 1717U }
};

/* =========================================================
 * Melody — Twinkle Twinkle Little Star
 * Uses only notes from the C-major scale above.
 *
 * C C G G A A G  (Twinkle twinkle little star)
 * F F E E D D C  (How I wonder what you are)
 * G G F F E E D  (Up above the world so high)
 * G G F F E E D  (Like a diamond in the sky)
 * C C G G A A G  (Twinkle twinkle little star)
 * F F E E D D C  (How I wonder what you are)
 * ========================================================= */
static const Note_t melody[] = {
    /* Twinkle twinkle little star */
    {"C4",3436U},{"C4",3436U},{"G4",2295U},{"G4",2295U},
    {"A4",2044U},{"A4",2044U},{"G4",2295U},
    /* How I wonder what you are */
    {"F4",2572U},{"F4",2572U},{"E4",2727U},{"E4",2727U},
    {"D4",3061U},{"D4",3061U},{"C4",3436U},
    /* Up above the world so high */
    {"G4",2295U},{"G4",2295U},{"F4",2572U},{"F4",2572U},
    {"E4",2727U},{"E4",2727U},{"D4",3061U},
    /* Like a diamond in the sky */
    {"G4",2295U},{"G4",2295U},{"F4",2572U},{"F4",2572U},
    {"E4",2727U},{"E4",2727U},{"D4",3061U},
    /* Twinkle twinkle little star */
    {"C4",3436U},{"C4",3436U},{"G4",2295U},{"G4",2295U},
    {"A4",2044U},{"A4",2044U},{"G4",2295U},
    /* How I wonder what you are */
    {"F4",2572U},{"F4",2572U},{"E4",2727U},{"E4",2727U},
    {"D4",3061U},{"D4",3061U},{"C4",3436U}
};

/* =========================================================
 * Main
 * ========================================================= */
int main(void)
{
    SystemClock_Config();
    USART2_Init();
    TIM2_Init();          /* starts ms_count via NVIC  */
    TIM4_PWM_Init();      /* sets up PB6 PWM output    */

    USART2_SendString("\r\n===== Task 6A: Passive Buzzer — C-Major Scale =====\r\n");
    USART2_SendString(
        "Note   | ARR    | CCR1   | f_actual\r\n"
        "-------|--------|--------|----------\r\n");

    /* ── Part 1: Play C-major scale, 400 ms per note ── */
    USART2_SendString("\r\n-- C-Major Scale --\r\n");

    uint32_t n_scale = sizeof(scale) / sizeof(scale[0]);
    for (uint32_t i = 0; i < n_scale; i++) {
        Buzzer_PlayNote(scale[i].name, scale[i].arr, 400U);
        Buzzer_Silence(50U);   /* 50 ms gap between notes */
    }

    delay_ms(500U);   /* pause between scale and melody */

    /* ── Part 2: Play melody — Twinkle Twinkle ── */
    USART2_SendString("\r\n-- Melody: Twinkle Twinkle Little Star --\r\n");

    uint32_t n_melody = sizeof(melody) / sizeof(melody[0]);
    for (uint32_t i = 0; i < n_melody; i++) {
        Buzzer_PlayNote(melody[i].name, melody[i].arr, 200U);
        Buzzer_Silence(40U);
    }

    Buzzer_Silence(0U);   /* ensure buzzer off at end */
    USART2_SendString("\r\n===== Task 6A Complete =====\r\n");

    while (1) {}
}

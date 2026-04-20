/*
 * ============================================================
 * CSE 2206 — Microcontroller & Embedded System Lab-02
 * Task 5: WS2812B RGB LED — Colour Mixing and Animation
 *         BARE-METAL Implementation
 * Platform : STM32F446RE Nucleo-64
 * Clock    : fTIM1_CLK = 180 MHz (APB2 × 2), tick = 5.56 ns
 *
 * Pin: PA8 → TIM1_CH1 (AF1) → WS2812B DIN
 *
 * Protocol:
 *   PSC=0, ARR=224 → bit period = 225 ticks = 1250 ns
 *   T1H = CCR1=144  → 144×5.56 = 800.6 ns  (logic 1 high time)
 *   T0H = CCR1=72   → 72×5.56  = 400.3 ns  (logic 0 high time)
 *   RES >50 µs (data line LOW after last bit)
 * ============================================================
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "helper.h"

/* Common helpers */
extern void SystemClock_Config(void);
extern void USART2_Init(void);
extern void USART2_SendString(const char *s);
extern void TIM6_Init(void);
extern void delay_us(uint16_t us);
extern void delay_ms(uint32_t ms);

/* =========================================================
 * WS2812B TIMING CONSTANTS (fTIM1 = 180 MHz, PSC=0)
 *   1 tick = 5.556 ns
 *   ARR  = 224  → bit period = 225 ticks = 1250 ns
 *   T1H  = 144 ticks ≈ 800 ns   (±150 ns tolerance: 650–950 ns) ✓
 *   T0H  = 72  ticks ≈ 400 ns   (±150 ns tolerance: 250–550 ns) ✓
 *   RESET: PA8 LOW for ≥50 µs
 * ========================================================= */
#define WS_ARR     224U    /* One full bit period: 225 ticks = 1250 ns */
#define WS_T1H     144U    /* Logic-1 high time: 144 ticks ≈ 800 ns    */
#define WS_T0H      72U    /* Logic-0 high time:  72 ticks ≈ 400 ns    */
#define WS_RESET_US  55U   /* Reset pulse low time (>50 µs)             */

/* =========================================================
 * SECTION 1 — TIM1 CH1 One-Pulse Mode Initialisation (Algorithm A5.1)
 *
 * One-pulse mode (OPM): after a single ARR overflow the counter
 * stops automatically (CEN cleared by hardware). This gives a
 * precise PWM pulse with no software timing for the falling edge.
 * ========================================================= */

/**
 * @brief  Initialise TIM1 CH1 for WS2812B single-wire protocol.
 *         PA8 configured as AF1 (TIM1_CH1).
 */
static void TIM1_WS2812_Init(void)
{
    /* Step 1: Enable GPIOA and TIM1 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    __NOP(); __NOP();

    /* Step 2: Configure PA8 — AF mode, Push-Pull, Very High Speed, No Pull */
    GPIOA->MODER  &= ~(3U << (8*2));
    GPIOA->MODER  |=  (2U << (8*2));       /* Alternate Function */
    GPIOA->OTYPER &= ~(1U << 8);           /* Push-Pull          */
    GPIOA->OSPEEDR|=  (3U << (8*2));       /* Very High Speed    */
    GPIOA->PUPDR  &= ~(3U << (8*2));       /* No pull            */

    /* PA8 is in AFR[1] (pins 8–15), pin 8 occupies bits [3:0] of AFR[1] */
    GPIOA->AFR[1] &= ~(0xFU << ((8-8)*4));
    GPIOA->AFR[1] |=  (0x1U << ((8-8)*4));  /* AF1 = TIM1 */

    /* Step 3a: One-Pulse Mode — counter stops after one ARR overflow */
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->CR1 |= TIM_CR1_OPM;

    /* Step 3b: PSC = 0 (no prescaling — full 180 MHz) */
    TIM1->PSC = 0U;

    /* Step 3c: ARR = 224 → bit period = 225 ticks = 1250 ns */
    TIM1->ARR = WS_ARR;

    /* Step 3d: Force shadow register reload */
    TIM1->EGR = TIM_EGR_UG;

    /* Step 4: Configure CH1 for PWM Mode 1 with preload */
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM1->CCMR1 |=  (6U << TIM_CCMR1_OC1M_Pos);   /* PWM Mode 1 */
    TIM1->CCMR1 |= TIM_CCMR1_OC1PE;                 /* Preload enable */

    /* Step 5: Enable CH1 output, active-high polarity */
    TIM1->CCER |= TIM_CCER_CC1E;
    TIM1->CCER &= ~TIM_CCER_CC1P;

    /* Step 6: MANDATORY for TIM1/TIM8 — enable Main Output */
    TIM1->BDTR |= TIM_BDTR_MOE;
}

/* =========================================================
 * SECTION 2 — PA8 GPIO Toggle for Reset Pulse
 *
 * After 24 bits, PA8 must be held LOW for ≥50 µs.
 * We temporarily switch PA8 from AF back to GPIO Output.
 * ========================================================= */

/** @brief  Switch PA8 to plain GPIO output (LOW) for reset pulse. */
static void PA8_SetGPIO_Low(void)
{
    GPIOA->MODER &= ~(3U << (8*2));
    GPIOA->MODER |=  (1U << (8*2));    /* GPIO Output mode */
    GPIOA->ODR   &= ~(1U << 8);        /* Drive LOW        */
}

/** @brief  Restore PA8 to AF1 (TIM1_CH1) after reset pulse. */
static void PA8_RestoreAF1(void)
{
    GPIOA->MODER &= ~(3U << (8*2));
    GPIOA->MODER |=  (2U << (8*2));    /* Back to Alternate Function */
}

/* =========================================================
 * SECTION 3 — Bit Transmission (Algorithm A5.2)
 * ========================================================= */

/**
 * @brief  Transmit a single WS2812B bit using TIM1 one-pulse mode.
 * @param  bit  1 → T1H pulse, 0 → T0H pulse.
 *
 * One-pulse mode automatically clears CEN after one ARR overflow,
 * so we simply load CCR1, start the timer, and poll for CEN=0.
 */
static void WS_SendBit(uint8_t bit)
{
    /* Step 1: Load the appropriate high-time into CCR1 */
    TIM1->CCR1 = (bit != 0U) ? WS_T1H : WS_T0H;

    /* Step 2: Reset counter to ensure clean start */
    TIM1->CNT = 0U;

    /* Step 3: Start the timer (one-pulse mode) */
    TIM1->CR1 |= TIM_CR1_CEN;

    /* Step 4: Wait for one-pulse to complete (CEN auto-clears on overflow) */
    while (TIM1->CR1 & TIM_CR1_CEN) {}

    /* Step 5: Clear Update Interrupt Flag */
    TIM1->SR &= ~TIM_SR_UIF;
}

/* =========================================================
 * SECTION 4 — Colour Frame Transmission (Algorithm A5.3)
 * ========================================================= */

/**
 * @brief  Send one WS2812B LED colour frame (24 bits, GRB order).
 * @param  r  Red channel   (0–255).
 * @param  g  Green channel (0–255).
 * @param  b  Blue channel  (0–255).
 *
 * Wire order: Green MSB first, then Red, then Blue.
 */
static void WS2812_SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    /* Step 1: Disable interrupts to prevent timing corruption.
     *         Any ISR latency during bit transmission corrupts the
     *         pulse width and is misinterpreted by the WS2812B.    */
    __disable_irq();

    /* Step 2: Transmit GREEN byte, MSB first */
    for (int i = 7; i >= 0; i--)
    {
        WS_SendBit((g >> i) & 1U);
    }

    /* Step 3: Transmit RED byte, MSB first */
    for (int i = 7; i >= 0; i--)
    {
        WS_SendBit((r >> i) & 1U);
    }

    /* Step 4: Transmit BLUE byte, MSB first */
    for (int i = 7; i >= 0; i--)
    {
        WS_SendBit((b >> i) & 1U);
    }

    /* Step 5: Re-enable interrupts */
    __enable_irq();

    /* Step 6: Reset pulse — hold data line LOW for ≥50 µs
     *         to latch the 24-bit frame into the LED.           */
    PA8_SetGPIO_Low();
    delay_us(WS_RESET_US);
    PA8_RestoreAF1();
}

/* =========================================================
 * SECTION 5 — Multi-LED Chain Support
 *
 * For a chain of N LEDs, send N × 24 bits consecutively,
 * then issue a single reset pulse. LED1 consumes bits 1–24,
 * LED2 consumes bits 25–48, and so on.
 * ========================================================= */

#define MAX_LEDS  8U

typedef struct {
    uint8_t r, g, b;
} LED_Color_t;

/**
 * @brief  Send colours to a chain of LEDs.
 * @param  colors  Array of LED_Color_t structs.
 * @param  count   Number of LEDs in the chain.
 */
static void WS2812_SetChain(const LED_Color_t *colors, uint32_t count)
{
    __disable_irq();

    for (uint32_t led = 0; led < count; led++)
    {
        /* Green byte */
        for (int i = 7; i >= 0; i--)
            WS_SendBit((colors[led].g >> i) & 1U);
        /* Red byte */
        for (int i = 7; i >= 0; i--)
            WS_SendBit((colors[led].r >> i) & 1U);
        /* Blue byte */
        for (int i = 7; i >= 0; i--)
            WS_SendBit((colors[led].b >> i) & 1U);
    }

    __enable_irq();

    /* Single reset pulse latches all LEDs */
    PA8_SetGPIO_Low();
    delay_us(WS_RESET_US);
    PA8_RestoreAF1();
}

/* =========================================================
 * SECTION 6 — HSV → RGB Conversion (Algorithm A5.4)
 *
 * Simplified integer HSV (S=1, V=1) for hue sweep.
 * Hue H ∈ [0, 359] degrees.
 * ========================================================= */

/**
 * @brief  Convert HSV hue to RGB and transmit to the LED.
 * @param  H  Hue angle in degrees (0–359).
 */
static void WS2812_SetHue(uint16_t H)
{
    H = H % 360U;

    uint8_t seg  = (uint8_t)(H / 60U);           /* Sextant 0–5 */
    uint8_t frac = (uint8_t)(H % 60U);            /* Fractional remainder */
    uint8_t q    = (uint8_t)(255U * (60U - frac) / 60U);  /* Falling ramp */
    uint8_t t    = (uint8_t)(255U * frac / 60U);           /* Rising ramp  */

    uint8_t r, g, b;

    switch (seg)
    {
        case 0:  r=255; g=t;   b=0;   break;   /* Red → Yellow  */
        case 1:  r=q;   g=255; b=0;   break;   /* Yellow → Green */
        case 2:  r=0;   g=255; b=t;   break;   /* Green → Cyan  */
        case 3:  r=0;   g=q;   b=255; break;   /* Cyan → Blue   */
        case 4:  r=t;   g=0;   b=255; break;   /* Blue → Magenta */
        case 5:  r=255; g=0;   b=q;   break;   /* Magenta → Red  */
        default: r=0;   g=0;   b=0;   break;
    }

    WS2812_SetColor(r, g, b);
}

/* =========================================================
 * SECTION 7 — Colour Palette (Table 3 from Assignment)
 * ========================================================= */

typedef struct {
    const char *name;
    uint8_t r, g, b;
} Colour_Entry_t;

/* All 10 colours from Table 3 */
static const Colour_Entry_t palette[] =
{
    {"Red",       255,   0,   0},
    {"Green",       0, 255,   0},
    {"Blue",        0,   0, 255},
    {"Yellow",    255, 255,   0},
    {"Cyan",        0, 255, 255},
    {"Magenta",   255,   0, 255},
    {"White",     255, 255, 255},
    {"Warm White",255, 200,  80},
    {"DU Blue",    31,  56, 100},
    {"Off",         0,   0,   0},
};

#define PALETTE_COUNT  (sizeof(palette) / sizeof(palette[0]))

/* =========================================================
 * SECTION 8 — Main Demonstration
 * ========================================================= */

int main(void)
{
    char buf[128];

    SystemClock_Config();
    USART2_Init();
    TIM6_Init();
    TIM1_WS2812_Init();

    USART2_SendString("\r\n===== Lab-02 Task 5: WS2812B (Bare-Metal) =====\r\n");

    /* ── Demo 1: Cycle through all 10 palette colours ── */
    USART2_SendString("\r\n[Demo 1] Colour palette (1 s per colour)\r\n");

    for (uint32_t i = 0; i < PALETTE_COUNT; i++)
    {
        WS2812_SetColor(palette[i].r, palette[i].g, palette[i].b);

        /* Print GRB wire encoding per assignment format */
        snprintf(buf, sizeof(buf),
            "Colour: %-12s R=%3u G=%3u B=%3u  GRB=[%02X %02X %02X]\r\n",
            palette[i].name,
            palette[i].r, palette[i].g, palette[i].b,
            palette[i].g, palette[i].r, palette[i].b);
        USART2_SendString(buf);

        delay_ms(1000U);
    }

    /* ── Demo 2: Full hue sweep H=0..359, step 3, 25 ms/step ── */
    USART2_SendString("\r\n[Demo 2] Hue sweep 0-359 degrees (step 3, 25 ms/step)\r\n");

    for (uint16_t h = 0; h < 360U; h += 3U)
    {
        WS2812_SetHue(h);
        delay_ms(25U);
    }
    USART2_SendString("Hue sweep complete.\r\n");

    /* ── Demo 3 (Extension): 4-LED colour-chase animation ── */
    USART2_SendString("\r\n[Demo 3] 4-LED colour chase (red rotates, rest off)\r\n");

    LED_Color_t chain[4] = {0};   /* All off initially */

    for (int round = 0; round < 3; round++)   /* 3 full rotations */
    {
        for (uint32_t active = 0; active < 4U; active++)
        {
            /* Clear all LEDs */
            for (int j = 0; j < 4; j++)
                chain[j] = (LED_Color_t){0, 0, 0};

            /* Illuminate active LED in red */
            chain[active] = (LED_Color_t){255, 0, 0};

            WS2812_SetChain(chain, 4U);

            snprintf(buf, sizeof(buf),
                "  Active LED: %lu/4\r\n", (unsigned long)(active + 1U));
            USART2_SendString(buf);

            delay_ms(200U);
        }
    }

    /* Turn all off */
    for (int j = 0; j < 4; j++) chain[j] = (LED_Color_t){0, 0, 0};
    WS2812_SetChain(chain, 4U);

    USART2_SendString("\r\n===== Task 5 Complete =====\r\n");

    while (1) {}
}

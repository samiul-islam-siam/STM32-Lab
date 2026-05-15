/*
 * CSE 2206: Lab-02
 * Task 5 - WS2812B RGB LED: Colour Mixing and Animation
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Modnal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "helper.h"

#define WS_ARR      225U    /* TIM1 Auto-Reload (ARR): 226 ticks = 1.256 µs */
#define WS_T1H      144U    /* Logic-1 high time: 144 ticks = 0.8 µs */
#define WS_T0H       72U    /* Logic-0 high time:  75 ticks = 0.4 µs */
#define WS_RESET     50U    /* Reset: 50 × zero entries → ≥ 50 µs line LOW */
#define NUM_LEDS      5U    /* Physical LED chain length */

#define PWM_BUF_SIZE   ((NUM_LEDS * 24U) + WS_RESET)

static uint16_t pwmData[PWM_BUF_SIZE];

typedef struct {
	uint8_t r, g, b;
} LED_t;

static LED_t g_leds[NUM_LEDS];

static void GPIO_WS_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    __NOP(); __NOP();

    /* PA8: Alternate Function, Push-Pull, Very High Speed, No Pull */
    GPIOA->MODER   = (GPIOA->MODER  & ~(3U << 16)) | (2U << 16);
    GPIOA->OTYPER  &= ~(1U << 8);
    GPIOA->OSPEEDR |=  (3U << 16);
    GPIOA->PUPDR   &= ~(3U << 16);

    /* AFR[1]: PA8 (pin 8-8=0 offset) → AF1 = TIM1_CH1 */
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0xFU) | 0x1U;
}

static void TIM1_WS_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    __NOP(); __NOP();

    TIM1->CR1  = 0U;           /* Counter disabled (CEN=0)                    */
    TIM1->PSC  = 0U;           /* No prescaler → 180 MHz timer clock          */
    TIM1->ARR  = WS_ARR;       /* Auto-reload = 225 → 226 ticks per period    */
    TIM1->CCR1 = 0U;           /* Compare = 0 → output stays LOW at idle      */
    TIM1->RCR  = 0U;           /* No repetition counter                       */

    /* CH1: PWM Mode 1 (OC1M=6) + preload enable (OC1PE) */
    TIM1->CCMR1 = (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;

    /* CH1 output enable, active-high polarity */
    TIM1->CCER  = TIM_CCER_CC1E;

    /* Main Output Enable — MANDATORY for TIM1/TIM8 */
    TIM1->BDTR  = TIM_BDTR_MOE;

    /* Force shadow registers to load, then clear all flags */
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR  = 0U;
}

static void DMA2_WS_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    __NOP(); __NOP();

    /* Disable stream before configuration (mandatory per RM §10.3.3) */
    DMA2_Stream1->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream1->CR & DMA_SxCR_EN) {}   /* Wait HW confirms */

    /* Clear all Stream1 interrupt flags */
    DMA2->LIFCR = DMA_LIFCR_CTCIF1  |
                  DMA_LIFCR_CHTIF1  |
                  DMA_LIFCR_CTEIF1  |
                  DMA_LIFCR_CDMEIF1 |
                  DMA_LIFCR_CFEIF1;

    /*
     * CR register:
     *   CHSEL [27:25] = 6     Channel 6 → TIM1_CH1 DMA request
     *   PL    [17:16] = 2     High priority
     *   MSIZE [14:13] = 1     Memory data width  = 16-bit
     *   PSIZE [12:11] = 1     Peripheral width   = 16-bit (TIM1->CCR1)
     *   MINC  [10]    = 1     Memory auto-increment after each beat
     *   PINC  [9]     = 0     Peripheral address fixed (always CCR1)
     *   CIRC  [8]     = 0     Single (non-circular) mode
     *   DIR   [7:6]   = 01    Memory-to-Peripheral
     *   TCIE  [4]     = 0     NOT set — polling used instead of interrupt
     *   EN    [0]     = 0     Stream disabled (enabled per frame)
     */
    DMA2_Stream1->CR =
          (6U << 25U)    /* CHSEL = 6  */
        | (2U << 16U)    /* PL    = 2  */
        | (1U << 13U)    /* MSIZE = 1  */
        | (1U << 11U)    /* PSIZE = 1  */
        | (1U << 10U)    /* MINC  = 1  */
        | (1U <<  6U);   /* DIR   = 01 */
                         /* TCIE intentionally omitted — polling mode */

    /* Peripheral destination: TIM1->CCR1 (fixed, never changes) */
    DMA2_Stream1->PAR = (uint32_t)&TIM1->CCR1;

    /* Direct mode (DMDIS=0) — default, no FIFO buffering */
    DMA2_Stream1->FCR &= ~DMA_SxFCR_DMDIS;

    /* NVIC NOT configured — no interrupt handler registered */
}

static void WS2812B_Send(void)
{
    uint32_t idx = 0;

    /* ── Step 1: Build bit-stream (GRB order, MSB first) ── */
    for (uint32_t led = 0; led < NUM_LEDS; led++)
    {
        /* Pack GRB into 24-bit value */
        uint32_t color = ((uint32_t)g_leds[led].g << 16)
                       | ((uint32_t)g_leds[led].r <<  8)
                       |  (uint32_t)g_leds[led].b;

        for (int bit = 23; bit >= 0; bit--)
            pwmData[idx++] = (color & (1U << bit)) ? WS_T1H : WS_T0H;
    }

    /* ── Step 2: Append reset pulse (≥50 µs: 50 × 1.256 µs = 62.8 µs) ── */
    for (uint32_t i = 0; i < WS_RESET; i++)
        pwmData[idx++] = 0U;

    /* ── Step 3: Stop TIM1 if somehow still running ── */
    TIM1->CR1 &= ~TIM_CR1_CEN;

    /* ── Step 4: Reconfigure DMA stream ── */

    /* Must disable stream before writing NDTR and M0AR */
    DMA2_Stream1->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream1->CR & DMA_SxCR_EN) {}

    /* Clear all Stream1 flags from previous frame */
    DMA2->LIFCR = DMA_LIFCR_CTCIF1  |
                  DMA_LIFCR_CHTIF1  |
                  DMA_LIFCR_CTEIF1  |
                  DMA_LIFCR_CDMEIF1 |
                  DMA_LIFCR_CFEIF1;

    DMA2_Stream1->M0AR = (uint32_t)pwmData;  /* Source buffer        */
    DMA2_Stream1->NDTR = idx;                /* Total 16-bit beats   */

    /* ── Step 5: Reset TIM1 counter and status ── */
    TIM1->CCR1 = 0U;
    TIM1->CNT  = 0U;
    TIM1->SR   = 0U;

    /* ── Step 6: Enable TIM1 CH1 DMA request (CC1DE) ── */
    TIM1->DIER |= TIM_DIER_CC1DE;

    /* ── Step 7: Enable DMA stream ── */
    DMA2_Stream1->CR |= DMA_SxCR_EN;

    /* ── Step 8: Start TIM1 — first CC1 event triggers first DMA beat ── */
    TIM1->CR1 |= TIM_CR1_CEN;

    /* ── Step 9: Poll Transfer-Complete flag
     */
    while (!(DMA2->LISR & DMA_LISR_TCIF1)) {}

    /* ── Step 10: Inline cleanup (was previously done inside the ISR) ── */
    DMA2->LIFCR       = DMA_LIFCR_CTCIF1;   /* Clear TC flag                 */
    TIM1->CR1        &= ~TIM_CR1_CEN;        /* Stop TIM1 counter             */
    DMA2_Stream1->CR &= ~DMA_SxCR_EN;        /* Disable DMA stream            */
    TIM1->DIER       &= ~TIM_DIER_CC1DE;     /* Disable TIM1 CH1 DMA request  */
}

static void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint32_t i = 0; i < NUM_LEDS; i++)
        g_leds[i] = (LED_t){r, g, b};
    WS2812B_Send();
}

static void WS2812_SetHue(uint16_t H)
{
    H = H % 360U;
    uint8_t seg  = (uint8_t)(H / 60U);
    uint8_t frac = (uint8_t)(H % 60U);
    uint8_t q    = (uint8_t)(255U * (60U - frac) / 60U);
    uint8_t t    = (uint8_t)(255U * frac          / 60U);
    uint8_t r, g, b;

    switch (seg)
    {
        case 0: r=255; g=t;   b=0;   break;
        case 1: r=q;   g=255; b=0;   break;
        case 2: r=0;   g=255; b=t;   break;
        case 3: r=0;   g=q;   b=255; break;
        case 4: r=t;   g=0;   b=255; break;
        case 5: r=255; g=0;   b=q;   break;
        default: r=0; g=0; b=0; break;
    }
    WS2812_SetAll(r, g, b);
}

typedef struct {
	const char *name; uint8_t r, g, b;
} Colour_t;

static const Colour_t palette[] =
{
    {"Red",            255,   0,   0},
    {"Green",            0, 255,   0},
    {"Blue",             0,   0, 255},
    {"Yellow",         255, 255,   0},
    {"Cyan",             0, 255, 255},
    {"Magenta",        255,   0, 255},
    {"White",          255, 255, 255},
    {"Warm White",     255, 200,  80},
    {"Cool White",     215, 235, 255},
    {"DU Blue",         31,  56, 100},
    {"Off",              0,   0,   0}
};
#define PALETTE_COUNT  (sizeof(palette) / sizeof(palette[0]))

int main(void)
{
    char buf[128];

    SystemClock_Config();
    USART2_Init();
    TIM6_Init();
    GPIO_WS_Init();
    TIM1_WS_Init();
    DMA2_WS_Init();

    USART2_SendString("\r\n===== Task 5: WS2812B Colour Mixing and Animation (Bare-Metal) =====\r\n");

    /* Power-on clear — all 5 LEDs OFF */
    WS2812_SetAll(0, 0, 0);
    delay_ms(100U);

    /*
     * Req 1: 10-colour palette, 1 sec per colour (all 5 LEDs)
     */
    USART2_SendString("\r\n[1] Colour palette — 1 sec per colour\r\n");

    for (uint32_t i = 0; i < PALETTE_COUNT; i++)
    {
        WS2812_SetAll(palette[i].r, palette[i].g, palette[i].b);

        snprintf(buf, sizeof(buf),
            "Colour: %-12s R=%3u G=%3u B=%3u  GRB=[%02X %02X %02X]\r\n",
            palette[i].name,
            palette[i].r, palette[i].g, palette[i].b,
            palette[i].g, palette[i].r, palette[i].b);
        USART2_SendString(buf);

        delay_ms(1000U);
    }

    /*
     * Req 2: Hue sweep 0–359°, step 3, 25 ms/step
     */
    USART2_SendString("\r\n[2] Hue sweep 0-359 (step 3, 25 ms/step)\r\n");

    for (uint16_t h = 0; h < 360U; h += 3U)
    {
        WS2812_SetHue(h);
        delay_ms(25U);
    }
    WS2812_SetAll(0, 0, 0);
    USART2_SendString("Hue sweep complete.\r\n");

    /*
     * Req 3 (Extension): 4-LED colour-chase, 3 rounds
     *   LED 0-3: one red LED rotates every 200 ms
     *   LED 4  : always OFF
     */
    USART2_SendString("\r\n[3] Colour chase — 5 LEDs, 3 rounds\r\n");

    for (int round = 0; round < 3; round++)
    {
        for (uint32_t active = 0; active < 5U; active++)
        {
            for (uint32_t j = 0; j < NUM_LEDS; j++)
                g_leds[j] = (LED_t){0, 0, 0};

            g_leds[active] = (LED_t){255, 0, 0};

            WS2812B_Send();

            snprintf(buf, sizeof(buf),
                "  Round %d — Active LED: %lu/4\r\n",
                round + 1, (unsigned long)(active + 1U));
            USART2_SendString(buf);

            delay_ms(200U);
        }
    }

    WS2812_SetAll(0, 0, 0);
    USART2_SendString("\r\n===== Task 5 Complete =====\r\n");

    while (1) {}
}

/*
 * WS2812B RGB LED
 * Din: Connect to D7
 */

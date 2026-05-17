#include "stm32f446xx.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * ============================================================
 * WS2812B Driver - STM32F4 @ 16MHz HSI
 * ============================================================
 * IMPORTANT SETTINGS IN STM32CubeIDE:
 *   Optimization: -O0  (Project > Properties > C/C++ Build >
 *                        Settings > MCU GCC Compiler > Optimization)
 *
 * Wiring:
 *   PA8 --> 330 ohm resistor --> WS2812B Din
 *   LED strip VCC --> 5V
 *   LED strip GND --> STM32 GND (common ground is ESSENTIAL)
 *   100nF capacitor across each LED's VCC/GND pins
 * ============================================================
 */

/* ── Pin control ─────────────────────────────────────────── */
#define WS_HIGH()  (GPIOA->BSRR = (1U << 8))
#define WS_LOW()   (GPIOA->BSRR = (1U << 24))

/* ── NOP building blocks ─────────────────────────────────── */
/*
 * At 16MHz -O0: 1 NOP = 62.5ns, BSRR write + branch = ~4 cycles overhead
 *
 * T0H = 375ns  (2 NOPs + 4 overhead = 375ns)  spec: 250-550ns  PASS
 * T1H = 812ns  (9 NOPs + 4 overhead = 812ns)  spec: 650-950ns  PASS
 * T0L = 875ns  (10 NOPs + 4 overhead = 875ns) spec: 700-1000ns PASS
 * T1L = 437ns  (3 NOPs + 4 overhead = 437ns)  spec: 300-600ns  PASS
 *
 * Gap between T0H and T1H = 7 NOPs = 437ns -> LED decodes cleanly
 */
#define NOP1   __asm volatile ("nop")
#define NOP2   NOP1; NOP1
#define NOP3   NOP2; NOP1
#define NOP9   NOP3; NOP3; NOP3
#define NOP10  NOP9; NOP1

/* ── LED buffer ──────────────────────────────────────────── */
#define NUM_LEDS 5

typedef struct { uint8_t r, g, b; } Color;

static Color leds[NUM_LEDS];

/* ── UART buffer ─────────────────────────────────────────── */
static char uartbuf[128];

/* ── Prototypes ──────────────────────────────────────────── */
void clock_init(void);
void gpio_init(void);
void uart2_init(void);
void uart_print(const char *s);
void ws_send_byte(uint8_t byte);
void ws_show(void);
void ws_reset(void);
void delay_ms(uint32_t ms);
void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                uint8_t *r, uint8_t *g, uint8_t *b);
void demo_colors(void);
void demo_hue_sweep(void);
void demo_chase(void);

/* ── The 10 required colours (Table 4) ──────────────────── */
typedef struct {
    const char *name;
    uint8_t r, g, b;
} NamedColor;

static const NamedColor TABLE4[10] = {
    {"Red",     255,   0,   0},
    {"Green",     0, 255,   0},
    {"Blue",      0,   0, 255},
    {"Yellow",  255, 255,   0},
    {"Cyan",      0, 255, 255},
    {"Magenta", 255,   0, 255},
    {"White",   255, 255, 255},
    {"Orange",  255, 128,   0},
    {"Purple",  128,   0, 128},
    {"Pink",    255, 105, 180},
};

/* ─────────────────────────────────────────────────────────── */
int main(void)
{
    clock_init();
    gpio_init();
    uart2_init();

    /* Start with all LEDs off */
    memset(leds, 0, sizeof(leds));
    ws_show();
    delay_ms(200);

    uart_print("\r\n=============================\r\n");
    uart_print(" WS2812B 5-LED Strip Demo\r\n");
    uart_print("=============================\r\n\r\n");

    while (1)
    {
        demo_colors();
        demo_hue_sweep();
        demo_chase();
    }
}

/* ─────────────────────────────────────────────────────────── */
/*  DEMO 1: Cycle through all 10 Table-4 colours (req. 6.7.1) */
/* ─────────────────────────────────────────────────────────── */
void demo_colors(void)
{
    uart_print("--- Demo 1: Colour Cycle ---\r\n");

    for (int c = 0; c < 10; c++)
    {
        uint8_t r = TABLE4[c].r;
        uint8_t g = TABLE4[c].g;
        uint8_t b = TABLE4[c].b;

        /* Fill all 5 LEDs */
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = r;
            leds[i].g = g;
            leds[i].b = b;
        }
        ws_show();

        /* Exact UART format from requirement 6.7.1 */
        snprintf(uartbuf, sizeof(uartbuf),
            "Colour: %-8s R=%3d G=%3d B=%3d GRB=[%02X %02X %02X]\r\n",
            TABLE4[c].name, r, g, b, g, r, b);
        uart_print(uartbuf);

        delay_ms(1000);
    }

    /* Brief off between demos */
    memset(leds, 0, sizeof(leds));
    ws_show();
    delay_ms(300);
}

/* ─────────────────────────────────────────────────────────── */
/*  DEMO 2: Full hue sweep H=0..359 step 3 (req. 6.7.2)       */
/* ─────────────────────────────────────────────────────────── */
void demo_hue_sweep(void)
{
    uart_print("\r\n--- Demo 2: Hue Sweep ---\r\n");

    for (uint16_t h = 0; h < 360; h += 3)
    {
        uint8_t r, g, b;
        hsv_to_rgb(h, 255, 200, &r, &g, &b);

        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].r = r;
            leds[i].g = g;
            leds[i].b = b;
        }
        ws_show();
        delay_ms(25);
    }

    memset(leds, 0, sizeof(leds));
    ws_show();
    delay_ms(300);
}

/* ─────────────────────────────────────────────────────────── */
/*  DEMO 3: Red chase across 4 LEDs (req. 6.7.3 extension)    */
/*  Uses LEDs 0-3 only (4-LED chain as specified)             */
/* ─────────────────────────────────────────────────────────── */
void demo_chase(void)
{
    uart_print("\r\n--- Demo 3: Chase Animation ---\r\n");

    /* Run the chase for 3 full rotations */
    for (int rep = 0; rep < 3; rep++)
    {
        for (int active = 0; active < 4; active++)
        {
            /* All off, then light one red */
            memset(leds, 0, sizeof(leds));
            leds[active].r = 255;
            leds[active].g = 0;
            leds[active].b = 0;
            ws_show();

            snprintf(uartbuf, sizeof(uartbuf),
                "Chase: active LED = %d\r\n", active);
            uart_print(uartbuf);

            delay_ms(200);
        }
    }

    memset(leds, 0, sizeof(leds));
    ws_show();
    delay_ms(500);
}

/* ─────────────────────────────────────────────────────────── */
/*  WS2812B core — timing is everything here                   */
/* ─────────────────────────────────────────────────────────── */

/*
 * Send one bit with hard-coded NOP delays.
 * Do NOT call any function inside here — call overhead = dead timing.
 * Written as if/else (not ternary) so both branches compile identically.
 */
static inline void ws_bit(uint8_t b)
{
    if (b) {
        WS_HIGH(); NOP9;  WS_LOW(); NOP3;
    } else {
        WS_HIGH(); NOP2;  WS_LOW(); NOP10;
    }
}

/*
 * Send one byte MSB first.
 * Fully unrolled — a for-loop adds ~5 cycles per bit = breaks timing.
 */
void ws_send_byte(uint8_t b)
{
    ws_bit(b & 0x80);
    ws_bit(b & 0x40);
    ws_bit(b & 0x20);
    ws_bit(b & 0x10);
    ws_bit(b & 0x08);
    ws_bit(b & 0x04);
    ws_bit(b & 0x02);
    ws_bit(b & 0x01);
}

/*
 * Push leds[] to the strip.
 * Interrupts MUST be disabled — any IRQ during transmission
 * inserts dead time that the LED mistakes for a reset pulse,
 * causing only the first LED to latch and the rest to go dark.
 *
 * GRB order: Green byte first, then Red, then Blue.
 */
void ws_show(void)
{
    __disable_irq();
    for (int i = 0; i < NUM_LEDS; i++) {
        ws_send_byte(leds[i].g);
        ws_send_byte(leds[i].r);
        ws_send_byte(leds[i].b);
    }
    __enable_irq();

    /* Reset: hold LOW > 50us. Using a counted loop avoids SysTick dependency. */
    WS_LOW();
    /* 50us @ 16MHz = 800 cycles. Loop body = ~4 cycles -> 200 iterations safe */
    for (volatile uint32_t i = 0; i < 800; i++) __asm volatile("nop");
}

/* ─────────────────────────────────────────────────────────── */
/*  HSV → RGB conversion                                       */
/*  h: 0-359, s: 0-255, v: 0-255                              */
/* ─────────────────────────────────────────────────────────── */
void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) { *r = *g = *b = v; return; }

    uint8_t  region  = h / 60;
    uint32_t rem     = (h - region * 60) * 255 / 60;
    uint8_t  p = (uint32_t)v * (255 - s) / 255;
    uint8_t  q = (uint32_t)v * (255 - (s * rem) / 255) / 255;
    uint8_t  t = (uint32_t)v * (255 - (s * (255 - rem)) / 255) / 255;

    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

/* ─────────────────────────────────────────────────────────── */
/*  Peripheral init                                            */
/* ─────────────────────────────────────────────────────────── */

void clock_init(void)
{
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
    FLASH->ACR = FLASH_ACR_PRFTEN;
}

void gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    __DSB();

    /* PA8: output, push-pull, very high speed, no pull */
    GPIOA->MODER   &= ~(3U << (8 * 2));
    GPIOA->MODER   |=  (1U << (8 * 2));
    GPIOA->OTYPER  &= ~(1U << 8);
    GPIOA->OSPEEDR |=  (3U << (8 * 2));
    GPIOA->PUPDR   &= ~(3U << (8 * 2));
    WS_LOW();

    /* PA2: USART2 TX, AF7 */
    GPIOA->MODER  &= ~(3U << (2 * 2));
    GPIOA->MODER  |=  (2U << (2 * 2));
    GPIOA->AFR[0] &= ~(0xFU << (2 * 4));
    GPIOA->AFR[0] |=  (7U   << (2 * 4));
}

void uart2_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    USART2->BRR = 0x008B;   /* 115200 baud @ 16MHz */
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void uart_print(const char *s)
{
    while (*s) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = (uint8_t)*s++;
    }
}

void delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 4000; i++);
}

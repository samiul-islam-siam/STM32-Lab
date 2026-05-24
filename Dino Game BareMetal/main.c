/*
 * Bare-metal LCD Dino Game (STM32F446RE / Nucleo-F446RE)
 *
 * Author:
 *   Md. Samiul Islam Siam  (Roll: 02)
 *   Partho Kumar Mondal    (Roll: 07)
 *
 */

#include "main.h"
#include "lcd.h"
#include <stdlib.h>   /* rand(), srand() */

/* ============================================================
 * 1.  SysTick – 1 ms tick counter
 * ============================================================ */

volatile uint32_t tick_ms = 0;   /* incremented every 1 ms by SysTick_Handler */

void SysTick_Handler(void)
{
    tick_ms++;
}

static inline uint32_t get_tick(void) {
	return tick_ms;
}

/* ============================================================
 * 2.  LCD hardware mapping
 * ============================================================
 * Data bus D4-D7 → PC7, PB6, PA7, PA6
 * RS  → PB5
 * EN  → PB4
 * Buzzer → PB0
 * Button → PA0 (pull-down, active HIGH)
 */
Lcd_PortType dataPorts[] = { GPIOC, GPIOB, GPIOA, GPIOA };
Lcd_PinType  dataPins[]  = { GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_6 };

Lcd_HandleTypeDef lcd;

/* ============================================================
 * 3.  Custom character bitmaps
 * ============================================================ */
uint8_t DINO_RIGHT_FOOT_PART_1[8] = {0x00,0x00,0x02,0x02,0x03,0x03,0x01,0x01};
uint8_t DINO_RIGHT_FOOT_PART_2[8] = {0x07,0x07,0x07,0x04,0x1C,0x1C,0x18,0x00};
uint8_t DINO_LEFT_FOOT_PART_1[8]  = {0x00,0x00,0x02,0x02,0x03,0x03,0x01,0x00};
uint8_t DINO_LEFT_FOOT_PART_2[8]  = {0x07,0x07,0x07,0x04,0x1C,0x1C,0x18,0x08};
uint8_t CACTUS_PART_1[8]          = {0x00,0x04,0x04,0x14,0x14,0x1C,0x04,0x04};
uint8_t CACTUS_PART_2[8]          = {0x04,0x05,0x05,0x15,0x1F,0x04,0x04,0x04};
uint8_t BIRD_WINGS_PART_1[8]      = {0x01,0x01,0x01,0x01,0x09,0x1F,0x00,0x00};
uint8_t BIRD_WINGS_PART_2[8]      = {0x00,0x10,0x18,0x1C,0x1E,0x1F,0x00,0x00};

/* ============================================================
 * 4.  Game-state variables
 * ============================================================ */

/* Dino position */
int dino_col1 = 1;
int dino_col2 = 2;
int dino_row  = 1;

/* Leg animation */
uint32_t timer1 = 0;
int period1     = 100;
int flag        = 1;

/* Obstacle position */
int obstacle_row = 0;
int obstacle_col = 13;

/* Obstacle movement */
uint32_t timer2 = 0;
int period2     = 100;

/* Internal state flags */
int a = 0;   /* 1 = redraw obstacle this cycle         */
int b = 1;   /* left  collision column (unused detail) */
int c = 2;   /* right collision column (unused detail) */
int d = 0;   /* 0 = on ground, 1 = jumping             */

/* Score timing */
uint32_t timer3 = 0;
int period3     = 100;

/* Score values */
int score          = 0;
int score_hundreds = 0;

/* Obstacle type:  0 = bird,  1 = cactus A,  2 = cactus B */
int random_num = 0;

/* Bird position */
int bird_col = 13;

/* Speed control */
int f            = 13;
int acceleration = 1;

/* Jump sound timing */
uint32_t timer4 = 0;
int period4     = 800;

/* Button edge detection */
int btn_curr = 0;
int btn_prev = 0;

/* ============================================================
 * 5.  buzz_tone
 *     Bit-bang buzzer on PB0.
 * ============================================================ */
void buzz_tone(uint32_t frequency, uint32_t duration_ms)
{
    if (frequency == 0) {
        delay_ms(duration_ms);
        return;
    }

    /* half_period in µs */
    uint32_t half_period_us = 500000UL / frequency;

    /* number of full cycles that fit in duration_ms */
    uint32_t cycles = (duration_ms * 1000UL) / (2UL * half_period_us);

    for (uint32_t i = 0; i < cycles; i++) {
        GPIO_WritePin(GPIOB, GPIO_PIN_0, 1);
        /* Busy-wait: HSI 16 MHz ≈ 16 cycles/µs; loop body ~1 cycle */
        for (volatile uint32_t t = 0; t < (half_period_us * 16UL); t++);
        GPIO_WritePin(GPIOB, GPIO_PIN_0, 0);
        for (volatile uint32_t t = 0; t < (half_period_us * 16UL); t++);
    }
}

/* ============================================================
 * 6.  gameOver
 * ============================================================ */
void gameOver(void)
{
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 4);  Lcd_string(&lcd, "GAME OVER!");
    Lcd_cursor(&lcd, 1, 3);  Lcd_string(&lcd, "Score:");
    Lcd_cursor(&lcd, 1, 9);  Lcd_int(&lcd, (score_hundreds * 100) + score);
    while (1);   /* halt – press reset to play again */
}

/* ============================================================
 * 7.  Clock configuration  (HSI 16 MHz, no PLL)
 * ============================================================ */
void SystemClock_Config(void)
{
    /* Ensure HSI oscillator is on and stable */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    /* Select HSI as SYSCLK (SW = 00) – already default after reset */
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != 0x00U);   /* SWS=00 → HSI */
}

/* ============================================================
 * 8.  GPIO initialisation
 *
 *     Pin map:
 *       PA0        – input, pull-down  (button)
 *       PA6, PA7   – output PP         (LCD D6, D7)
 *       PB0        – output PP         (buzzer)
 *       PB4        – output PP         (LCD EN)
 *       PB5        – output PP         (LCD RS)
 *       PB6        – output PP         (LCD D5)
 *       PC7        – output PP         (LCD D4)
 * ============================================================ */
static void GPIO_Init(void)
{
    /* Enable peripheral clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                  | RCC_AHB1ENR_GPIOBEN
                  | RCC_AHB1ENR_GPIOCEN;

    /* Short NOP for clock stabilization */
    __NOP(); __NOP(); __NOP(); __NOP();

    /* ── GPIOA ── */

    /* PA0: input */
    GPIOA->MODER  &= ~(3U << (0U * 2U));   /* MODER[1:0] = 00 input */

    /* PA0: pull-down */
    GPIOA->PUPDR  &= ~(3U << (0U * 2U));
    GPIOA->PUPDR  |=  (2U << (0U * 2U));   /* PUPDR = 10 → pull-down */

    /* PA6, PA7: output push-pull, no pull */
    GPIOA->MODER  &= ~((3U << (6U * 2U)) | (3U << (7U * 2U)));
    GPIOA->MODER  |=  ((1U << (6U * 2U)) | (1U << (7U * 2U)));  /* 01 = output */
    GPIOA->OTYPER &= ~((1U << 6U) | (1U << 7U));                 /* push-pull */
    GPIOA->OSPEEDR &= ~((3U << (6U * 2U)) | (3U << (7U * 2U))); /* low speed  */
    GPIOA->PUPDR  &= ~((3U << (6U * 2U)) | (3U << (7U * 2U)));  /* no pull    */

    /* Initial state: reset */
    GPIOA->BSRR = ((uint32_t)GPIO_PIN_6 | (uint32_t)GPIO_PIN_7) << 16U;

    /* ── GPIOB ── */

    /* PB0, PB4, PB5, PB6: output push-pull, no pull */
    GPIOB->MODER  &= ~((3U << (0U * 2U)) | (3U << (4U * 2U))
                     | (3U << (5U * 2U)) | (3U << (6U * 2U)));
    GPIOB->MODER  |=  ((1U << (0U * 2U)) | (1U << (4U * 2U))
                     | (1U << (5U * 2U)) | (1U << (6U * 2U)));
    GPIOB->OTYPER &= ~((1U << 0U) | (1U << 4U) | (1U << 5U) | (1U << 6U));
    GPIOB->OSPEEDR &= ~((3U << (0U * 2U)) | (3U << (4U * 2U))
                      | (3U << (5U * 2U)) | (3U << (6U * 2U)));
    GPIOB->PUPDR  &= ~((3U << (0U * 2U)) | (3U << (4U * 2U))
                     | (3U << (5U * 2U)) | (3U << (6U * 2U)));

    /* Initial state: reset */
    GPIOB->BSRR = ((uint32_t)GPIO_PIN_0 | (uint32_t)GPIO_PIN_4
                 | (uint32_t)GPIO_PIN_5 | (uint32_t)GPIO_PIN_6) << 16U;

    /* ── GPIOC ── */

    /* PC7: output push-pull, no pull */
    GPIOC->MODER  &= ~(3U << (7U * 2U));
    GPIOC->MODER  |=  (1U << (7U * 2U));
    GPIOC->OTYPER &= ~(1U << 7U);
    GPIOC->OSPEEDR &= ~(3U << (7U * 2U));
    GPIOC->PUPDR  &= ~(3U << (7U * 2U));

    /* Initial state: reset */
    GPIOC->BSRR = (uint32_t)GPIO_PIN_7 << 16U;
}

/* ============================================================
 * 9.  main
 * ============================================================ */
int main(void)
{
    /* --- Clock ----------------------------------------------------------- */
    SystemClock_Config();   /* HSI 16 MHz confirmed */

    /* --- SysTick: 1 ms tick at 16 MHz ------------------------------------
     *   Reload = 16 000 000 / 1 000 - 1 = 15 999
     *   CTRL[2]=1 → processor clock source (not /8)
     *   CTRL[1]=1 → SysTick exception enabled
     *   CTRL[0]=1 → counter enabled
     * --------------------------------------------------------------------- */
    SysTick->LOAD = 16000UL - 1UL;
    SysTick->VAL  = 0UL;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk   /* AHB (processor) clock */
                  | SysTick_CTRL_TICKINT_Msk      /* enable SysTick_Handler */
                  | SysTick_CTRL_ENABLE_Msk;      /* start counter          */

    /* --- Peripherals ----------------------------------------------------- */
    GPIO_Init();

    lcd = Lcd_create(dataPorts, dataPins,
                     GPIOB, GPIO_PIN_5,   /* RS */
                     GPIOB, GPIO_PIN_4,   /* EN */
                     LCD_4_BIT_MODE);

    /* Define custom characters in CGRAM slots 0-7 */
    Lcd_define_char(&lcd, 0, DINO_RIGHT_FOOT_PART_1);
    Lcd_define_char(&lcd, 1, DINO_RIGHT_FOOT_PART_2);
    Lcd_define_char(&lcd, 2, DINO_RIGHT_FOOT_PART_1);
    Lcd_define_char(&lcd, 3, DINO_RIGHT_FOOT_PART_2);
    Lcd_define_char(&lcd, 4, DINO_LEFT_FOOT_PART_1);
    Lcd_define_char(&lcd, 5, DINO_LEFT_FOOT_PART_2);
    Lcd_define_char(&lcd, 6, CACTUS_PART_1);
    Lcd_define_char(&lcd, 7, CACTUS_PART_2);

    srand(get_tick());   /* seed RNG with elapsed ticks */

    /* =================================================================== */
    while (1)
    {
        uint32_t now = get_tick();   /* replaces HAL_GetTick() */

        /* ── Dino leg animation timer ─────────────────────────────────── */
        if (now > timer1 + (uint32_t)period1) {
            timer1 = now;
            flag = (flag == 1) ? 2 : 1;
        }

        /* ── Obstacle movement timer ──────────────────────────────────── */
        if (now > timer2 + (uint32_t)period2) {
            timer2 = now;
            obstacle_col--;
            if (obstacle_col < 0) {
                obstacle_col = 13;
                if (period2 > 30) period2 -= acceleration;
                random_num = rand() % 3;
            }
            f = obstacle_col + 1;
            /* Erase trailing column and column 0 on both rows */
            Lcd_cursor(&lcd, 1, f); Lcd_string(&lcd, " ");
            Lcd_cursor(&lcd, 0, f); Lcd_string(&lcd, " ");
            Lcd_cursor(&lcd, 1, 0); Lcd_string(&lcd, " ");
            Lcd_cursor(&lcd, 0, 0); Lcd_string(&lcd, " ");
            a = 1;
        }

        /* ── Draw dino (only when on ground) ──────────────────────────── */
        if (d == 0) {
            if (flag == 1) {
                Lcd_cursor(&lcd, dino_row, dino_col1); Lcd_write_char(&lcd, 2);
                Lcd_cursor(&lcd, dino_row, dino_col2); Lcd_write_char(&lcd, 3);
            } else {
                Lcd_cursor(&lcd, dino_row, dino_col1); Lcd_write_char(&lcd, 4);
                Lcd_cursor(&lcd, dino_row, dino_col2); Lcd_write_char(&lcd, 5);
            }
        }

        /* ── Draw obstacle ────────────────────────────────────────────── */
        if (a == 1) {
            if (random_num == 1) {
                /* Cactus type A – bottom row */
                obstacle_row = 1;
                Lcd_define_char(&lcd, 6, CACTUS_PART_1);
                Lcd_cursor(&lcd, obstacle_row, obstacle_col);
                Lcd_write_char(&lcd, 6);
            } else if (random_num == 2) {
                /* Cactus type B – bottom row */
                obstacle_row = 1;
                Lcd_define_char(&lcd, 7, CACTUS_PART_2);
                Lcd_cursor(&lcd, obstacle_row, obstacle_col);
                Lcd_write_char(&lcd, 7);
            } else {
                /* Bird – top row, two cells wide */
                bird_col     = obstacle_col - 1;
                obstacle_row = 0;
                Lcd_define_char(&lcd, 6, BIRD_WINGS_PART_1);
                Lcd_cursor(&lcd, obstacle_row, bird_col);
                Lcd_write_char(&lcd, 6);
                Lcd_define_char(&lcd, 7, BIRD_WINGS_PART_2);
                Lcd_cursor(&lcd, obstacle_row, obstacle_col);
                Lcd_write_char(&lcd, 7);
            }
            a = 0;
        }

        /* ── Collision detection ──────────────────────────────────────── */

        /* Bird: dino must be jumping when bird reaches column 1 */
        if (obstacle_row == 0 && d == 1 &&
            (obstacle_col == 1 || bird_col == 1)) {
            gameOver();
        }

        /* Cactus: dino must be on ground when cactus reaches column 1 */
        if (obstacle_row == 1 && d == 0 && obstacle_col == 1) {
            gameOver();
        }

        /* ── Jump logic ───────────────────────────────────────────────── */
        /*   Button PA0 held HIGH → dino jumps (stays on top row while held)
         *   Button released      → dino lands                              */
        if (GPIOA->IDR & GPIO_PIN_0) {          /* replaces HAL_GPIO_ReadPin */
            if (d == 0) {
                /* Clear bottom row dino when first leaving ground */
                Lcd_cursor(&lcd, 1, 0); Lcd_string(&lcd, "    ");
            }
            d = 1;
            Lcd_cursor(&lcd, 0, dino_col1); Lcd_write_char(&lcd, 2);
            Lcd_cursor(&lcd, 0, dino_col2); Lcd_write_char(&lcd, 3);
        } else {
            if (d == 1) {   /* just landed – erase top-row dino */
                Lcd_cursor(&lcd, 0, dino_col1); Lcd_string(&lcd, " ");
                Lcd_cursor(&lcd, 0, dino_col2); Lcd_string(&lcd, " ");
            }
            d = 0;
        }

        /* ── Score timer ──────────────────────────────────────────────── */
        if (now > timer3 + (uint32_t)period3) {
            timer3 = now;
            score++;
            if (score == 100) {
//                buzz_tone(800, 150);
//                buzz_tone(900, 150);
                score = 0;
                score_hundreds++;
                if (score_hundreds == 100) score_hundreds = 0;
            }
            Lcd_cursor(&lcd, 1, 14); Lcd_int(&lcd, score);
            Lcd_cursor(&lcd, 0, 14); Lcd_int(&lcd, score_hundreds);

            /* Button edge detection: clear dino ghost on state change */
            btn_curr = (GPIOA->IDR & GPIO_PIN_0) ? 1 : 0;
            if (btn_curr != btn_prev) {
                Lcd_cursor(&lcd, 0, 1); Lcd_string(&lcd, "  ");
            }
            btn_prev = btn_curr;
        }
    }
    /* never reached */
}

/* ============================================================
 * 10. Error handler
 * ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    while (1);
}

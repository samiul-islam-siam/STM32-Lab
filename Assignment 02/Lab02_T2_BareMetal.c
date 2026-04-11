/*
 * ============================================================
 * CSE 2206 — Microcontroller & Embedded System Lab-02
 * Task 2: Delay Generation (ms, s, Hours) — BARE-METAL
 * Platform : STM32F446RE Nucleo-64
 * Clock    : SYSCLK = 180 MHz, APB1 = 45 MHz, TIM6_CLK = 90 MHz
 * ============================================================
 *
 */

#include <stm32f446xx.h>   /* CMSIS device header — all RCC/GPIO/TIM structs */
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

/**
 * @brief  Second blocking delay.
 * @param  sec  Delay in whole seconds.
 */
static void delay_s(uint32_t sec)
{
    for (uint32_t i = 0; i < sec; i++)
    {
        delay_ms(1000U);   /* 1 s = 1000 ms */
    }
}

/**
 * @brief  Hour/minute/second blocking delay.
 * @param  h   Hours   (0–255).
 * @param  m   Minutes (0–255).
 * @param  s   Seconds (0–255).
 *
 * Maximum without 32-bit overflow:
 *   total_s_max = 0xFFFFFFFF = 4294967295 s ≈ 136 years
 */
static void delay_hms(uint8_t h, uint8_t m, uint8_t s)
{
    /* Use 32-bit accumulator to prevent overflow during multiplication */
    uint32_t total_s = (uint32_t)h * 3600U
                     + (uint32_t)m * 60U
                     + (uint32_t)s;
    delay_s(total_s);
}

/* =========================================================
 * SECTION 4 — LED (LD2 = PA5) Helpers
 * ========================================================= */

/**
 * @brief  Initialise PA5 as push-pull output for LD2.
 */
static void LED_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;   /* GPIOA already enabled above */
    GPIOA->MODER &= ~(3U << (5*2));
    GPIOA->MODER |=  (1U << (5*2));          /* Output mode */
}

static inline void LED_On(void)  { GPIOA->ODR |=  (1U << 5); }
static inline void LED_Off(void) { GPIOA->ODR &= ~(1U << 5); }
static inline void LED_Toggle(void) { GPIOA->ODR ^= (1U << 5); }

/* =========================================================
 * SECTION 5 — Main Demonstration
 * ========================================================= */

int main(void)
{
	SystemClock_Config();
    char buf[80];

    /* Initialise peripherals */
    USART2_Init();
    TIM6_Init();
    LED_Init();

    USART2_SendString("\r\n===== Lab-02 Task 2: Delay Generation (Bare-Metal) =====\r\n");

    /* --- Demo 1: delay_ms(500) --- */
    USART2_SendString("Starting 500 ms delay...\r\n");
    delay_ms(500U);
    USART2_SendString("Done. [500 ms elapsed]\r\n\r\n");

    /* --- Demo 2: delay_ms(1000) --- */
    USART2_SendString("Starting 1000 ms delay...\r\n");
    delay_ms(1000U);
    USART2_SendString("Done. [1000 ms elapsed]\r\n\r\n");

    /* --- Demo 3: LED toggle 10 times at 250 ms on / 250 ms off --- */
    USART2_SendString("LED toggle demo: 10 x (250ms ON / 250ms OFF)...\r\n");
    for (int i = 0; i < 10; i++)
    {
        LED_On();
        snprintf(buf, sizeof(buf), "  Toggle %2d/10: LED ON\r\n",  i + 1);
        USART2_SendString(buf);
        delay_ms(250U);

        LED_Off();
        snprintf(buf, sizeof(buf), "  Toggle %2d/10: LED OFF\r\n", i + 1);
        USART2_SendString(buf);
        delay_ms(250U);
    }
    USART2_SendString("LED toggle done.\r\n\r\n");

    /* --- Demo 4: delay_s(3) --- */
    USART2_SendString("Starting 3 s delay...\r\n");
    delay_s(3U);
    USART2_SendString("Done. [3 s elapsed]\r\n\r\n");

    /* --- Demo 5: delay_hms(0, 0, 5) --- */
    USART2_SendString("Starting delay_hms(0, 0, 5)  [5 seconds]...\r\n");
    delay_hms(0U, 0U, 5U);
    USART2_SendString("Done. [5 s elapsed]\r\n\r\n");

    USART2_SendString("===== Task 2 Complete =====\r\n");

    /* Infinite loop — nothing else to do */
    while (1) {}
}

/*
 * ============================================================
 * CSE 2206 — Microcontroller & Embedded System Lab-02
 * Task 3: Duration Measurement & Code Profiling — BARE-METAL
 * Platform : STM32F446RE Nucleo-64
 * Clock    : fCPU = 180 MHz, fTIM2_CLK = 90 MHz (APB1×2)
 * ============================================================
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief  Configure fCPU = 180 MHz
 */
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
 * @param  s = Pointer to string (must be null-terminated).
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
 * TIM6 Initialisation for 1 µs tick
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
 * Delay Functions
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

/* =========================================================
 * DWT Cycle Counter (Method A)
 *
 * ARM Cortex-M4 Data Watchpoint and Trace unit.
 * Increments every CPU clock cycle → resolution = 1/180 MHz = 5.56 ns.
 * Overflows at 2^32 cycles = ~23.9 s at 180 MHz.
 * ========================================================= */

/**
 * @brief  Enable and reset the DWT cycle counter.
 *         Must be called once before using DWT_GetCycles().
 */
static void DWT_Init(void)
{
    /* Step 1: Enable trace subsystem via DEMCR register */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Step 2: Reset cycle counter to zero */
    DWT->CYCCNT = 0U;

    /* Step 3: Enable the cycle counter */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/** @brief  Return current DWT cycle count. */
static inline uint32_t DWT_GetCycles(void)
{
    return DWT->CYCCNT;
}

/* =========================================================
 * SECTION 2 — TIM2 Free-Running Counter (Method B)
 *
 * TIM2 is a 32-bit general-purpose timer on APB1.
 * fTIM2_CLK = 90 MHz.
 * PSC = 89 → tick = 1 µs  (same principle as TIM6 in Task 2)
 * ARR = 0xFFFFFFFF → overflow every ~71.6 minutes.
 * ========================================================= */

/**
 * @brief  Initialise TIM2 as 1 µs free-running stopwatch.
 */
static void TIM2_FreeRun_Init(void)
{
    /* Enable TIM2 peripheral clock */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    __NOP(); __NOP();

    /* Disable counter before configuration */
    TIM2->CR1 &= ~TIM_CR1_CEN;

    /* PSC = 89 → 1 µs resolution (90 MHz / 90 = 1 MHz) */
    TIM2->PSC = 89U;

    /* ARR = max 32-bit value for maximum measurement range */
    TIM2->ARR = 0xFFFFFFFFU;

    /* Force shadow register update */
    TIM2->EGR = TIM_EGR_UG;

    /* Clear status flags */
    TIM2->SR = 0U;

    /* Start the free-running counter */
    TIM2->CR1 |= TIM_CR1_CEN;
}

/** @brief  Return current TIM2 µs counter value (32-bit). */
static inline uint32_t TIM2_GetMicros(void)
{
    return TIM2->CNT;
}

/* =========================================================
 * Time Conversion Helper
 *
 * Converts DWT cycle count to ns/µs/ms/s and prints via UART.
 * ========================================================= */

/**
 * @brief  Print a profiling result row to UART.
 * @param  label   Description of the profiled block (max 30 chars).
 * @param  cycles  Elapsed DWT cycles.
 * @param  tim2_us Elapsed TIM2 microseconds.
 */
static void Profile_Print(const char *label, uint32_t cycles, uint32_t tim2_us)
{
    char buf[160];

    /* Convert cycles: fCPU = 180 MHz, so 1 cycle = 5.555... ns
     * Use integer arithmetic: ns = cycles * 1000 / 180             */
    uint32_t ns   = (uint32_t)((uint64_t)cycles * 1000U / 180U);
    uint32_t us   = ns / 1000U;
    uint32_t ms   = us / 1000U;

    snprintf(buf, sizeof(buf),
        "%-30s | DWT: %9lu cyc | %8lu ns | %6lu us | %4lu ms | TIM2: %6lu us\r\n",
        label, (unsigned long)cycles,
        (unsigned long)ns, (unsigned long)us, (unsigned long)ms,
        (unsigned long)tim2_us);
    USART2_SendString(buf);
}

/* =========================================================
 * Code Blocks Under Test
 * ========================================================= */

/* ---- Block 1: Bubble Sort (worst case: reverse sorted array) ---- */

#define SORT_N  100U

static int sort_arr[SORT_N];

/**
 * @brief  Fill array in reverse order (worst case for bubble sort).
 */
static void BubbleSort_PrepareWorstCase(void)
{
    for (uint32_t i = 0; i < SORT_N; i++)
    {
        sort_arr[i] = (int)(SORT_N - i);   /* N, N-1, ..., 2, 1 */
    }
}

/**
 * @brief  Standard bubble sort — O(N²) comparisons.
 */
static void BubbleSort(void)
{
    int temp;
    for (uint32_t i = 0; i < SORT_N - 1U; i++)
    {
        for (uint32_t j = 0; j < SORT_N - 1U - i; j++)
        {
            if (sort_arr[j] > sort_arr[j + 1U])
            {
                temp           = sort_arr[j];
                sort_arr[j]    = sort_arr[j + 1U];
                sort_arr[j + 1U] = temp;
            }
        }
    }
}

/* ---- Block 3: Integer Square Root (Newton-Raphson) ---- */

/**
 * @brief  Integer square root using Newton-Raphson iteration.
 * @param  n  Input value.
 * @return    floor(sqrt(n)).
 */
static uint32_t isqrt(uint32_t n)
{
    if (n == 0U) return 0U;
    uint32_t x = n;
    uint32_t y = (x + 1U) / 2U;
    while (y < x)
    {
        x = y;
        y = (x + n / x) / 2U;
    }
    return x;
}

static volatile uint32_t isqrt_result;   /* Volatile prevents optimiser removal */

/**
 * @brief  Run isqrt on 1000 successive inputs.
 */
static void IsqrtBenchmark(void)
{
    for (uint32_t i = 0; i < 1000U; i++)
    {
        isqrt_result = isqrt(i * 7U + 1U);
    }
}

/* ---- Block 4: Memory Copy (byte-by-byte, 512 bytes) ---- */

static uint8_t src_buf[512U];
static uint8_t dst_buf[512U];

/**
 * @brief  Byte-by-byte memory copy — deliberately avoids memcpy()
 *         to expose per-byte overhead.
 */
static void MemCopy_ByteByByte(void)
{
    for (uint32_t i = 0; i < 512U; i++)
    {
        dst_buf[i] = src_buf[i];
    }
}

/* =========================================================
 * Main Profiling Sequence
 * ========================================================= */

/**
 * @brief  Macro to profile a code block with both DWT and TIM2.
 *         Creates local variables for start/end times and calls
 *         Profile_Print with the results.
 */
#define PROFILE(label, block)                          \
    do {                                               \
        uint32_t _t0_dwt  = DWT_GetCycles();           \
        uint32_t _t0_tim2 = TIM2_GetMicros();          \
        { block }                                      \
        uint32_t _t1_dwt  = DWT_GetCycles();           \
        uint32_t _t1_tim2 = TIM2_GetMicros();          \
        Profile_Print(label,                           \
            _t1_dwt  - _t0_dwt,                        \
            _t1_tim2 - _t0_tim2);                      \
    } while (0)

int main(void)
{
	SystemClock_Config();
    /* Initialise all peripherals */
    USART2_Init();
    TIM6_Init();
    DWT_Init();
    TIM2_FreeRun_Init();

    /* Fill source buffer with known pattern */
    for (uint32_t i = 0; i < 512U; i++) src_buf[i] = (uint8_t)(i & 0xFF);

    /* Print table header */
    USART2_SendString("\r\n===== Lab-02 Task 3: Code Profiling (Bare-Metal) =====\r\n");
    USART2_SendString(
        "Block                          | DWT Cycles   | ns       | us     | ms   | TIM2 us\r\n"
        "-------------------------------|--------------|----------|--------|------|--------\r\n");

    /* ── Block 1: Bubble sort worst case ── */
    BubbleSort_PrepareWorstCase();
    PROFILE("[1] Bubble sort N=100 (worst)",  BubbleSort(); );

    /* ── Block 2: delay_ms(100) — compare ~100000 µs expected ── */
    PROFILE("[2] delay_ms(100)",  delay_ms(100U); );

    /* ── Block 3: SendString 48 chars ──
     * The 48-char string is exactly:  "PROFILING: STM32F446RE USART2 @ 115200 baud OK!"
     * (48 characters including null at position 48 — we transmit 48 chars)            */
    PROFILE("[3] USART2_SendString 48 B",
        USART2_SendString("PROFILING: STM32F446RE USART2 @ 115200 baud OK!"); );

    /* ── Block 4: Integer square root x1000 ── */
    PROFILE("[4] isqrt() x1000 inputs",  IsqrtBenchmark(); );

    /* ── Block 5: Byte-by-byte memcpy 512 B ── */
    PROFILE("[5] MemCopy byte x512",  MemCopy_ByteByByte(); );

    USART2_SendString("\r\n===== Task 3 Complete =====\r\n");

    while (1) {}
}

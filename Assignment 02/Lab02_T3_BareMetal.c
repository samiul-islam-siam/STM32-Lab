/*
 * CSE 2206: Lab-02
 * Task-03:  Duration Measurement & Code Profiling
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Modnal (Roll: 07)
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <stdio.h>
#include "helper.h"

void TIM2_FreeRun_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    __NOP();
    __NOP();

    TIM2->CR1 &= ~TIM_CR1_CEN;

    TIM2->PSC = 89U;

    TIM2->ARR = 0xFFFFFFFFU;

    TIM2->EGR = TIM_EGR_UG;

    TIM2->SR = 0U;

    TIM2->CR1 |= TIM_CR1_CEN;
}

uint32_t TIM2_GetMicros(void)
{
    return TIM2->CNT;
}

void DWT_Init(void)
{
    /* Step 1: Enable trace subsystem via DEMCR register */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Step 2: Reset cycle counter to zero */
    DWT->CYCCNT = 0U;

    /* Step 3: Enable the cycle counter */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Return current DWT cycle count */
uint32_t DWT_GetCycles(void)
{
    return DWT->CYCCNT;
}

#define N  100U
static int arr[N];

void BubbleSort_PrepareWorstCase(void)
{
    for (uint32_t i = 0; i < N; i++)
    {
        arr[i] = (int)(N - i);   /* N, N-1, ..., 2, 1 */
    }
}

void BubbleSort(void)
{
    int temp;
    for (uint32_t i = 0; i < N - 1U; i++)
    {
        for (uint32_t j = 0; j < N - 1U - i; j++)
        {
            if (arr[j] > arr[j + 1U])
            {
                temp = arr[j];
                arr[j] = arr[j + 1U];
                arr[j + 1U] = temp;
            }
        }
    }
}

/* ---- Integer Square Root (Newton-Raphson) ---- */
uint32_t isqrt(uint32_t n)
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

volatile uint32_t isqrt_result;   /* Volatile prevents optimizer removal */

void IsqrtBenchmark(void)
{
    for (uint32_t i = 0; i < 1000U; i++)
    {
        isqrt_result = isqrt(i * 7U + 1U);
    }
}

/* ---- Memory Copy (byte-by-byte, 512 bytes) ---- */
static uint8_t src_buf[512U];
static uint8_t dst_buf[512U];

void MemCopy_ByteByByte(void)
{
    for (uint32_t i = 0; i < 512U; i++)
    {
        dst_buf[i] = src_buf[i];
    }
}

/**
 * Output Formatting
 *
 * num    - block number string, e.g. "[1]"
 * label  - block description, e.g. "Bubble sort N=100 (worst)"
 * method - "DWT " or "TIM2"
 * cycles - DWT elapsed cycles  (ignored when method is TIM2)
 * us_val - elapsed microseconds (from whichever method applies)
 */
void Profile_PrintRow(const char *num, const char *label, const char *method,
                      uint32_t cycles, uint32_t us_val) {
    char buf[160];

    /* Convert from microseconds to other units */
    uint32_t ns_val = us_val * 1000U;
    uint32_t ms_val = us_val / 1000U;

    /* For DWT rows also convert cycles -> ns more precisely:
       ns = cycles * 1000 / 180  (avoids float, uses 64-bit to prevent overflow) */
    uint32_t ns_precise = (uint32_t)((uint64_t)cycles * 1000U / 180U);

    if (cycles > 0U)  /* DWT row */
    {
        snprintf(buf, sizeof(buf),
        	"%-4s|%-25s |%-6s |%-12lu |%-12lu |%-10lu |%-6lu\r\n",
            num, label, method,
            (unsigned long)cycles,
            (unsigned long)ns_precise,
            (unsigned long)(ns_precise / 1000U),
            (unsigned long)(ns_precise / 1000000U));
    }
    else  /* TIM2 row — no cycle count */
    {
        snprintf(buf, sizeof(buf),
        	"%-4s|%-25s |%-6s |%-12lu |%-12lu |%-10lu |%-6lu\r\n",
            num, label, method,
            "—",
            (unsigned long)ns_val,
            (unsigned long)us_val,
            (unsigned long)ms_val);
    }

    USART2_SendString(buf);
}


/**
 * Macro: profile a block with BOTH methods, print two rows
 *
 * Usage:  PROFILE("[1]", "Bubble sort N=100", { BubbleSort(); });
 */
#define PROFILE(num, label, block)                                     \
    do {                                                               \
        uint32_t _t0_dwt  = DWT_GetCycles();                           \
        uint32_t _t0_tim2 = TIM2_GetMicros();                          \
        { block }                                                      \
        uint32_t _t1_dwt  = DWT_GetCycles();                           \
        uint32_t _t1_tim2 = TIM2_GetMicros();                          \
        uint32_t _cyc     = _t1_dwt  - _t0_dwt;                        \
        uint32_t _us_tim2 = _t1_tim2 - _t0_tim2;                       \
        /* DWT row: pass cycles > 0, us derived from cycles */         \
        Profile_PrintRow(num, label, "DWT ", _cyc, 0U);                \
        /* TIM2 row: pass cycles = 0 to signal TIM2 path    */         \
        Profile_PrintRow(num, label, "TIM2", 0U,   _us_tim2);          \
        USART2_SendString(                                             \
        "    |                          |       |             |             |           |      \r\n"); \
    } while (0)


int main(void)
{
    SystemClock_Config();
    USART2_Init();
    TIM6_Init();

    DWT_Init();
    TIM2_FreeRun_Init();

    /* Initialise source buffer */
    for (uint32_t i = 0; i < 512U; i++)
        src_buf[i] = (uint8_t)(i & 0xFFU);

    /* ── Table header (matches Deliverable T3 column layout) ── */
    USART2_SendString("\r\n");
    USART2_SendString("===== Task 3: Duration Measurement & Code Profiling (Bare-Metal) =====\r\n");
    USART2_SendString("\r\n");
    USART2_SendString(
        "#   |Block Description         |Method |Cycles       |ns           |us         |ms    \r\n"
        "----|--------------------------|-------|-------------|-------------|-----------|------\r\n");

    /* ── [1] Bubble sort worst case ── */
    BubbleSort_PrepareWorstCase();
    PROFILE("[1]", "Bubble sort N=100 (worst)",
        BubbleSort();
    );

    /* ── [2] delay_ms(100)  — expected ~100 000 us ── */
    PROFILE("[2]", "delay_ms(100)",
        delay_ms(100U);
    );

    /* ── [3] SendString 48 chars  — effective baud check ──
       String: "PROFILING: STM32F446RE USART2 @ 115200 baud OK!"
       Length: 48 chars (47 visible + null; we transmit 47 bytes)
       Expected time at 115200 8N1: 47 * 10 bits / 115200 = ~4080 us  */
    PROFILE("[3]", "SendString 48B",
        USART2_SendString("PROFILING: STM32F446RE USART2 @ 115200 baud OK!\r\n");
    );

    /* ── [4] Integer sqrt x 1000 inputs ── */
    PROFILE("[4]", "isqrt() x1000 inputs",
        IsqrtBenchmark();
    );

    /* ── [5] Byte-by-byte memcpy 512 B ── */
    PROFILE("[5]", "MemCopy byte x512",
        MemCopy_ByteByByte();
    );

    USART2_SendString(
        "----|--------------------------|-------|-------------|-------------|-----------|------\r\n");
    USART2_SendString("===== Task 3 Complete =====\r\n\r\n");

    while (1) {}
}

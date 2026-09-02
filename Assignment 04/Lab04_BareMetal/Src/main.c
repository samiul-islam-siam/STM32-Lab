/* =====================================================================
 * CSE 2206 - Lab 04: ADC Multi-Resolution Testing with Flash Data Logging
 *
 * Implements Milestones:
 *   A - Raw ADC acquisition
 *   B - Flash utility functions
 *   C - Student identity storage
 *   D - Multi-resolution ADC test suite
 *   E - Boot sequence & result display
 *   F - Persistence & Robustness
 *
 * Author: Md. Samiul Islam Siam (Roll: 02)
 *         Partho Kumar Mondal (Roll: 07)
 * ===================================================================== */

#include <stm32f446xx.h>
#include <string.h>
#include <stdio.h>

#include "helper.h"

/* ---------------------------------------------------------------------
 * Config
 * --------------------------------------------------------------------- */
#define VREF_MV              3300U   /* mV, Nucleo VDDA ~ 3.3V          */
#define N_SAMPLES            16      /* averaged samples per resolution */

#define FLASH_SECTOR6_BASE   0x08040000U   /* identity block (write-once) */
#define FLASH_SECTOR7_BASE   0x08060000U   /* test-results block          */
#define FLASH_SECTOR6_NUM    6U
#define FLASH_SECTOR7_NUM    7U

#define IDENTITY_MARKER      0xB1010001U
#define RESULTS_MARKER       0xCAFEBABEU
#define ERASED_WORD          0xFFFFFFFFU

#define UART_DEBOUNCE_MS     300U     /* swallow bursts from one keypress */

/* ---------------------------------------------------------------------
 * Data layout
 * --------------------------------------------------------------------- */
typedef struct {
    uint32_t marker;          /* IDENTITY_MARKER if valid */
    char     registration[16];
    char     roll[12];
    char     name[32];
} StudentInfo_t;              /* 64 bytes, word-aligned   */

typedef struct {
    uint32_t      marker;     /* IDENTITY_MARKER if this block was provisioned */
    StudentInfo_t student[2];
} IdentityBlock_t;

typedef struct {
    uint32_t marker;          /* RESULTS_MARKER if valid  */
    uint32_t mv_12bit;        /* stored as millivolts     */
    uint32_t mv_10bit;
    uint32_t mv_8bit;
    uint32_t mv_6bit;
} TestResults_t;              /* 20 bytes -> 5 words      */

typedef enum {
	RES_12BIT = 0,
	RES_10BIT,
	RES_8BIT,
	RES_6BIT,
	RES_COUNT
} Resolution_t;

typedef struct {
    const char *label;
    uint32_t    rescfg;      /* register RES field value, 0..3 */
    uint32_t    maxcode;
} ResInfo_t;

static const ResInfo_t kResTable[RES_COUNT] = {
    { "12-bit", 0x0, 4095 },  /* RES = 00 */
    { "10-bit", 0x1, 1023 },  /* RES = 01 */
    { "8-bit",  0x2,  255 },  /* RES = 10 */
    { "6-bit",  0x3,   63 },  /* RES = 11 */
};

static void FPU_Enable(void)
{
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    __DSB();
    __ISB();
}

/* =====================================================================
 * Milestone A - ADC acquisition
 * ===================================================================== */

static void ADC_Init(void)
{
    /* Enable GPIOA + ADC1 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* PA0 -> analog mode (MODER = 11) */
    GPIOA->MODER |= (GPIO_MODER_MODER0_0 | GPIO_MODER_MODER0_1);

    /* Channel 0 first (and only) conversion in the regular sequence */
    ADC1->SQR3 = 0;
    ADC1->SQR1 &= ~ADC_SQR1_L;            /* L = 0 -> 1 conversion   */

    /* Sampling time for channel 0: ~56 cycles
     * Channel 0 lives in SMPR2 (channels 0-9). */
    ADC1->SMPR2 &= ~(0x7U << (3 * 0));
    ADC1->SMPR2 |=  (0x4U << (3 * 0));

    /* Default resolution 12-bit (RES = 00) */
    ADC1->CR1 &= ~ADC_CR1_RES;

    /* Right-aligned data (default), single conversion, no continuous mode */
    ADC1->CR2 &= ~ADC_CR2_CONT;
    ADC1->CR2 &= ~ADC_CR2_ALIGN;

    /* Power up ADC and wait for t_STAB */
    ADC1->CR2 |= ADC_CR2_ADON;
    delay_ms(1);
}

static void ADC_SetResolution(Resolution_t r)
{
    ADC1->CR1 = (ADC1->CR1 & ~ADC_CR1_RES) | (kResTable[r].rescfg << ADC_CR1_RES_Pos);
}

static uint16_t ADC_ReadRaw(void)
{
    ADC1->SR &= ~ADC_SR_EOC;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC)) { /* poll EOC */ }
    return (uint16_t)ADC1->DR;
}

/* =====================================================================
 * Milestone B - Flash utility functions
 * ===================================================================== */

#define FLASH_KEY1   0x45670123U
#define FLASH_KEY2   0xCDEF89ABU

static void Flash_Unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

static void Flash_Lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static void Flash_WaitBusy(void)
{
    while (FLASH->SR & FLASH_SR_BSY) { /* poll */ }
}

/* sectorNum: 0-7 on the F446RE (Sector 6 = identity, Sector 7 = results) */
static void Flash_EraseSector(uint8_t sectorNum)
{
    Flash_Unlock();
    Flash_WaitBusy();

    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= (0x2U << FLASH_CR_PSIZE_Pos);   /* PSIZE = 10: 32-bit, needs 2.7-3.6V */

    FLASH->CR &= ~FLASH_CR_SNB;
    FLASH->CR |= (sectorNum << FLASH_CR_SNB_Pos);
    FLASH->CR |= FLASH_CR_SER;
    FLASH->CR |= FLASH_CR_STRT;

    Flash_WaitBusy();

    FLASH->CR &= ~FLASH_CR_SER;
    FLASH->CR &= ~FLASH_CR_SNB;

    Flash_Lock();
}

static void Flash_WriteWord(uint32_t addr, uint32_t data)
{
    Flash_Unlock();
    Flash_WaitBusy();

    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= (0x2U << FLASH_CR_PSIZE_Pos);   /* 32-bit programming */
    FLASH->CR |= FLASH_CR_PG;

    *(volatile uint32_t *)addr = data;

    Flash_WaitBusy();

    FLASH->CR &= ~FLASH_CR_PG;
    Flash_Lock();
}

static uint32_t Flash_ReadWord(uint32_t addr)
{
    return *(volatile uint32_t *)addr; /* Flash is memory-mapped */
}

/* Write an arbitrary buffer as consecutive 32-bit words.
 * len is rounded up to a multiple of 4;
 * caller must ensure the target sector was erased first.
 * Used for both identity and results blocks. */
static void Flash_WriteBlock(uint32_t addr, const void *src, uint32_t len)
{
    uint32_t words = (len + 3) / 4;
    uint32_t buf;
    const uint8_t *p = (const uint8_t *)src;

    for (uint32_t i = 0; i < words; i++) {
        buf = 0xFFFFFFFFU;
        uint32_t remaining = len - (i * 4);
        uint32_t chunk = (remaining >= 4) ? 4 : remaining;
        memcpy(&buf, p + (i * 4), chunk);
        Flash_WriteWord(addr + (i * 4), buf);
    }
}

/* =====================================================================
 * Milestone C - Student identity (Sector 6, write-once)
 * ===================================================================== */

static void Identity_FillSlot(StudentInfo_t *slot, const char *reg,
                              const char *roll, const char *name)
{
    memset(slot, 0, sizeof(*slot));
    slot->marker = IDENTITY_MARKER;
    strncpy(slot->registration, reg,  sizeof(slot->registration) - 1);
    strncpy(slot->roll,         roll, sizeof(slot->roll) - 1);
    strncpy(slot->name,         name, sizeof(slot->name) - 1);
}

static void Identity_ProvisionPair(const char *reg0, const char *roll0, const char *name0,
                                   const char *reg1, const char *roll1, const char *name1)
{
    IdentityBlock_t block;
    memset(&block, 0, sizeof(block));
    block.marker = IDENTITY_MARKER;
    Identity_FillSlot(&block.student[0], reg0, roll0, name0);
    Identity_FillSlot(&block.student[1], reg1, roll1, name1);

    Flash_EraseSector(FLASH_SECTOR6_NUM);
    Flash_WriteBlock(FLASH_SECTOR6_BASE, &block, sizeof(block));
}

static void Identity_Display(void)
{
    char line[80];
    const IdentityBlock_t *block = (const IdentityBlock_t *)FLASH_SECTOR6_BASE;

    USART2_SendString("\r\n========== Student Identity ==========\r\n");
    if (block->marker == IDENTITY_MARKER) {
        for (int i = 0; i < 2; i++) {
            const StudentInfo_t *info = &block->student[i];
            if (info->marker != IDENTITY_MARKER) {
                continue;
            }
            snprintf(line, sizeof(line), "----- Member %d -----\r\n", i + 1);
            USART2_SendString(line);
            snprintf(line, sizeof(line), "Registration: %s\r\n", info->registration);
            USART2_SendString(line);
            snprintf(line, sizeof(line), "Roll:         %s\r\n", info->roll);
            USART2_SendString(line);
            snprintf(line, sizeof(line), "Name:         %s\r\n", info->name);
            USART2_SendString(line);
        }
    } else {
        USART2_SendString("Not yet provisioned.\r\n");
    }
}

/* =====================================================================
 * Milestone D - Multi-resolution ADC test suite (Sector 7)
 * ===================================================================== */

static float CodeToVolts(uint32_t avgCode, uint32_t maxCode)
{
    /* Floating point math */
    return ((float)avgCode * (float)VREF_MV / 1000.0f) / (float)maxCode;
}

static void RunTestSuite(void)
{
    TestResults_t results;
    memset(&results, 0, sizeof(results));
    results.marker = RESULTS_MARKER;

    USART2_SendString("\r\nRunning multi-resolution test suite...\r\n");

    for (Resolution_t r = RES_12BIT; r < RES_COUNT; r++) {
        ADC_SetResolution(r);

        /* allow resolution change to settle before sampling */
        delay_ms(1);

        uint32_t sum = 0;
        for (int s = 0; s < N_SAMPLES; s++) {
            sum += ADC_ReadRaw();
        }
        uint32_t avgCode = sum / N_SAMPLES;
        float volts = CodeToVolts(avgCode, kResTable[r].maxcode);

        char line[64];
        snprintf(line, sizeof(line), "%s: avg_code=%lu  V=%.3f\r\n",
                 kResTable[r].label, (unsigned long)avgCode, volts);
        USART2_SendString(line);

        switch (r) {
            case RES_12BIT: results.mv_12bit = (uint32_t)(volts * 1000.0f); break;
            case RES_10BIT: results.mv_10bit = (uint32_t)(volts * 1000.0f); break;
            case RES_8BIT:  results.mv_8bit  = (uint32_t)(volts * 1000.0f); break;
            case RES_6BIT:  results.mv_6bit  = (uint32_t)(volts * 1000.0f); break;
            default: break;
        }
    }

    /* restore 12-bit as the default resting resolution */
    ADC_SetResolution(RES_12BIT);

    Flash_EraseSector(FLASH_SECTOR7_NUM);
    Flash_WriteBlock(FLASH_SECTOR7_BASE, &results, sizeof(results));

    USART2_SendString("Results stored to Sector 7.\r\n");
}

/* =====================================================================
 * Milestone E - Boot sequence & result display
 * ===================================================================== */

static void Results_Display(void)
{
    char line[64];
    const TestResults_t *res = (const TestResults_t *)FLASH_SECTOR7_BASE;

    USART2_SendString("====== Previous Testing Results ======\r\n");
    if (res->marker == RESULTS_MARKER) {
        uint32_t vals[4] = { res->mv_12bit, res->mv_10bit, res->mv_8bit, res->mv_6bit };
        for (int i = 0; i < RES_COUNT; i++) {
        	snprintf(line, sizeof(line), "%s: %.3f V\r\n",
        	         kResTable[i].label, (float)vals[i] / 1000.0f);
            USART2_SendString(line);
        }
    } else {
        USART2_SendString("No previous test data.\r\n");
    }
}

static uint8_t UART2_ReadCharBlocking(void)
{
    uint8_t b;
    while (!USART2_TryReceiveByte(&b)) { /* wait */ }
    return b;
}

/* Debounce a UART trigger burst: once a byte is seen, ignore any further
 * bytes that arrive within UART_DEBOUNCE_MS. */
static void WaitAndDebounceTrigger(void)
{
    (void)UART2_ReadCharBlocking();   /* first byte = the trigger */

    uint32_t start = millis();
    while ((millis() - start) < UART_DEBOUNCE_MS) {
        uint8_t junk;
        if (USART2_TryReceiveByte(&junk)) {
            start = millis();   /* burst still going -- reset the window */
        }
    }
}

/* =====================================================================
 * main
 * ===================================================================== */

int main(void)
{
    FPU_Enable();
    SystemClock_Config();
    TIM2_Init();
    USART2_Init();
    ADC_Init();

    USART2_SendString("\r\n\r\n----- CSE2206 Lab 4: ADC & Flash -----\r\n\r\n");

    /* ---- One time provisioning path -------------------------------
     * Uncomment ONCE to provision identities and reflash, then comment it back out.
     * This is intentional to NOT run it automatically after every boot.
     * ----------------------------------------------------------------*/

//    Identity_ProvisionPair("2023-915-945", "2", "Md. Samiul Islam Siam",
//                           "2023-315-950", "7", "Partho Kumar Mondal");

    /* Boot sequence (Milestone E) */
    Identity_Display();
    Results_Display();

    USART2_SendString("\r\nSend any byte over UART to run the test suite...\r\n");

    while (1) {
        WaitAndDebounceTrigger();
        RunTestSuite();
        USART2_SendString("\r\nSend any byte to run again...\r\n");
    }
}

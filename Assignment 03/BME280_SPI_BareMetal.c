/*
 * CSE 2206 - Microcontroller and Embedded System
 * Lab Assignment 3 - Part A: BME280 via SPI (Bare-Metal / Register-Level)
 * Platform : STM32F446RE Nucleo-64
 * UART     : USART2 @ 115200 baud (PA2=TX, PA3=RX)
 * SPI      : SPI2  (PC1=MOSI, PC2=MISO, PC7=SCK, PB9=CS)
 * Timer    : TIM6  @ 1-second interrupt
 */

#include "stm32f446xx.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  BME280 Register Map                                                 */
/* ------------------------------------------------------------------ */
#define BME280_REG_CHIP_ID    0xD0
#define BME280_REG_RESET      0xE0
#define BME280_REG_CTRL_HUM   0xF2
#define BME280_REG_CTRL_MEAS  0xF4
#define BME280_REG_CONFIG     0xF5
#define BME280_REG_PRESS_MSB  0xF7

/* ------------------------------------------------------------------ */
/*  BME280 Calibration Storage                                          */
/* ------------------------------------------------------------------ */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5;
static int16_t  dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1;
static int16_t  dig_H2;
static uint8_t  dig_H3;
static int16_t  dig_H4, dig_H5;
static int8_t   dig_H6;

/* Compensated results (global, updated in TIM6 ISR) */
static float temp_C, temp_F, pres_hPa, hum_RH;
static int32_t t_fine;  /* shared between T and P compensation */

/* ------------------------------------------------------------------ */
/*  Simple software delay (rough)                                       */
/* ------------------------------------------------------------------ */
static void delay_ms(uint32_t ms)
{
    /* At 180 MHz, ~180000 NOPs ≈ 1 ms (conservative) */
    for (uint32_t i = 0; i < ms * 18000; i++) {
        __asm("NOP");
    }
}

/* ==================================================================
   STEP A3.1 — Clock Configuration (180 MHz via HSE PLL)
   ================================================================== */
static void SystemClock_Config(void)
{
    /* 1. Enable HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* 2. Configure PLL: source=HSE, PLLM=4, PLLN=180, PLLP=2 */
    RCC->PLLCFGR = (4  << RCC_PLLCFGR_PLLM_Pos)   /* PLLM = 4  */
                 | (180 << RCC_PLLCFGR_PLLN_Pos)   /* PLLN = 180*/
                 | (0   << RCC_PLLCFGR_PLLP_Pos)   /* PLLP = 2 (00) */
                 | RCC_PLLCFGR_PLLSRC_HSE;         /* HSE source */

    /* 3. Flash latency: 5 wait states for 180 MHz */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS
               | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN
               | FLASH_ACR_DCEN;

    /* 4. AHB=/1, APB1=/4, APB2=/2 */
    RCC->CFGR = RCC_CFGR_HPRE_DIV1   /* AHB  = 180 MHz */
              | RCC_CFGR_PPRE1_DIV4  /* APB1 =  45 MHz */
              | RCC_CFGR_PPRE2_DIV2; /* APB2 =  90 MHz */

    /* 5. Enable PLL and wait */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* 6. Switch SYSCLK to PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* ==================================================================
   STEP A3.2 — GPIO Configuration
   ================================================================== */
static void GPIO_Config(void)
{
    /* Enable clocks: GPIOA, GPIOB, GPIOC */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                  | RCC_AHB1ENR_GPIOBEN
                  | RCC_AHB1ENR_GPIOCEN;

    /* --- PA2 (USART2 TX) --- */
    GPIOA->MODER   |=  (2U << (2*2));   /* Alternate Function */
    GPIOA->AFR[0]  |=  (7U << (2*4));   /* AF7 = USART2       */
    GPIOA->OSPEEDR |=  (3U << (2*2));   /* High speed         */

    /* --- PA3 (USART2 RX) --- */
    GPIOA->MODER   |=  (2U << (3*2));
    GPIOA->AFR[0]  |=  (7U << (3*4));
    GPIOA->OSPEEDR |=  (3U << (3*2));

    /* --- PC1 (SPI2 MOSI) — AF7 --- */
    GPIOC->MODER   |=  (2U << (1*2));
    GPIOC->AFR[0]  |=  (7U << (1*4));
    GPIOC->OSPEEDR |=  (3U << (1*2));

    /* --- PC2 (SPI2 MISO) — AF5 --- */
    GPIOC->MODER   |=  (2U << (2*2));
    GPIOC->AFR[0]  |=  (5U << (2*4));
    GPIOC->OSPEEDR |=  (3U << (2*2));

    /* --- PC7 (SPI2 SCK) — AF5 --- */
    GPIOC->MODER   |=  (2U << (7*2));
    GPIOC->AFR[0]  |=  (5U << (7*4));
    GPIOC->OSPEEDR |=  (3U << (7*2));

    /* --- PB9 (CS) — GPIO Output, push-pull, initially HIGH --- */
    GPIOB->MODER   |=  (1U << (9*2));   /* General-purpose output */
    GPIOB->OTYPER  &= ~(1U << 9);       /* Push-pull              */
    GPIOB->OSPEEDR |=  (3U << (9*2));   /* High speed             */
    GPIOB->ODR     |=  (1U << 9);       /* CS HIGH (deselected)   */
}

/* ==================================================================
   STEP A3.3 — USART2 Configuration (115200 baud @ APB1 = 45 MHz)
   ================================================================== */
static void USART2_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* BRR: DIV_Mantissa=24, DIV_Fraction=7 → 115200 @ 45 MHz */
    USART2->BRR = (24U << 4) | 7U;

    /* Enable USART, TX, RX */
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void UART_SendString(const char *s)
{
    while (*s) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = (uint8_t)(*s++);
    }
}

/* ==================================================================
   STEP A3.4 — TIM6 (1-second interrupt @ APB1 = 45 MHz,
               but TIM6 runs on x2 = 90 MHz when APB1 prescaler ≠ 1)
   ================================================================== */
static void TIM6_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    /* PSC = 9000 → tick = 0.1 ms  (PCLK1 timer clock = 90 MHz) */
    TIM6->PSC = 9000 - 1;
    /* ARR = 10000 → interrupt every 1 s */
    TIM6->ARR = 10000 - 1;

    TIM6->DIER |= TIM_DIER_UIE;   /* Update Interrupt Enable */
    TIM6->CR1  |= TIM_CR1_CEN;    /* Counter Enable          */

    NVIC_SetPriority(TIM6_DAC_IRQn, 2);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/* ==================================================================
   STEP A3.5 — SPI2 Configuration
   ================================================================== */
static void SPI2_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    SPI2->CR1 = (1U << 2)   /* MSTR   = 1  : master mode     */
              | (2U << 3)   /* BR[2:0]= 010: fPCLK/8         */
              | (0U << 1)   /* CPOL   = 0                    */
              | (0U << 0)   /* CPHA   = 0  → Mode 00         */
              | (0U << 7)   /* LSBFIRST= 0 : MSB first       */
              | (1U << 9)   /* SSM    = 1  : software NSS    */
              | (1U << 8)   /* SSI    = 1                    */
              | (0U << 11); /* DFF    = 0  : 8-bit frame     */

    SPI2->CR1 |= (1U << 6); /* SPE = 1 : enable SPI          */
}

/* ==================================================================
   SPI Helper Functions
   ================================================================== */
static uint8_t SPI_TxRx(uint8_t data)
{
    while (!(SPI2->SR & SPI_SR_TXE));  /* Wait TXE  */
    SPI2->DR = data;
    while (!(SPI2->SR & SPI_SR_RXNE)); /* Wait RXNE */
    return (uint8_t)SPI2->DR;
}

static void BME280_SPI_WriteReg(uint8_t reg, uint8_t data)
{
    GPIOB->ODR &= ~(1U << 9);          /* CS LOW  */
    SPI_TxRx(reg & 0x7F);              /* MSB=0: write */
    SPI_TxRx(data);
    GPIOB->ODR |=  (1U << 9);          /* CS HIGH */
}

static void BME280_SPI_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    GPIOB->ODR &= ~(1U << 9);          /* CS LOW  */
    SPI_TxRx(reg | 0x80);              /* MSB=1: read  */
    for (uint8_t i = 0; i < len; i++)
        buf[i] = SPI_TxRx(0xFF);
    GPIOB->ODR |=  (1U << 9);          /* CS HIGH */
}

/* ==================================================================
   STEP A3.6 — BME280 Initialisation (SPI)
   ================================================================== */
static void BME280_ReadCalibration(void)
{
    uint8_t buf[26];

    /* --- Temperature & Pressure (0x88–0x9F = 24 bytes) --- */
    BME280_SPI_ReadRegs(0x88, buf, 24);
    dig_T1 = (uint16_t)(buf[1]  << 8) | buf[0];
    dig_T2 = (int16_t) (buf[3]  << 8) | buf[2];
    dig_T3 = (int16_t) (buf[5]  << 8) | buf[4];
    dig_P1 = (uint16_t)(buf[7]  << 8) | buf[6];
    dig_P2 = (int16_t) (buf[9]  << 8) | buf[8];
    dig_P3 = (int16_t) (buf[11] << 8) | buf[10];
    dig_P4 = (int16_t) (buf[13] << 8) | buf[12];
    dig_P5 = (int16_t) (buf[15] << 8) | buf[14];
    dig_P6 = (int16_t) (buf[17] << 8) | buf[16];
    dig_P7 = (int16_t) (buf[19] << 8) | buf[18];
    dig_P8 = (int16_t) (buf[21] << 8) | buf[20];
    dig_P9 = (int16_t) (buf[23] << 8) | buf[22];

    /* --- Humidity H1 (0xA1) --- */
    BME280_SPI_ReadRegs(0xA1, buf, 1);
    dig_H1 = buf[0];

    /* --- Humidity H2–H6 (0xE1–0xE7 = 7 bytes) --- */
    BME280_SPI_ReadRegs(0xE1, buf, 7);
    dig_H2 = (int16_t)(buf[1] << 8) | buf[0];
    dig_H3 = buf[2];
    dig_H4 = (int16_t)(buf[3] << 4) | (buf[4] & 0x0F);
    dig_H5 = (int16_t)(buf[5] << 4) | (buf[4] >> 4);
    dig_H6 = (int8_t) buf[6];
}

static void BME280_Init_SPI(void)
{
    uint8_t id;

    /* Soft reset */
    BME280_SPI_WriteReg(BME280_REG_RESET, 0xB6);
    delay_ms(10);

    /* Chip ID check */
    BME280_SPI_ReadRegs(BME280_REG_CHIP_ID, &id, 1);
    {
        char s[64];
        sprintf(s, "[A1] ChipID=0x%02X (expect 0x60)\r\n", id);
        UART_SendString(s);
    }

    /* Read calibration */
    BME280_ReadCalibration();

    /* Configure: osrs_h=x1 */
    BME280_SPI_WriteReg(BME280_REG_CTRL_HUM, 0x01);

    /* Configure: osrs_t=x2, osrs_p=x16, mode=normal */
    BME280_SPI_WriteReg(BME280_REG_CTRL_MEAS, 0x57);

    /* Configure: filter=16, t_sb=0.5ms */
    BME280_SPI_WriteReg(BME280_REG_CONFIG, 0x10);

    delay_ms(100);
}

/* ==================================================================
   STEP A3.7 — Data Read and Compensation
   ================================================================== */

/* BME280 datasheet integer compensation — temperature */
static int32_t BME280_CompensateTemp(int32_t adc_T)
{
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1)))
            * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1))
            * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
            * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8; /* in hundredths of °C */
}

/* BME280 datasheet integer compensation — pressure */
static uint32_t BME280_CompensatePressure(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8)
         + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)dig_P1) >> 33;
    if (var1 == 0) return 0;
    p  = 1048576 - adc_P;
    p  = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)p; /* in Q24.8 Pa → divide by 256 for Pa */
}

/* BME280 datasheet integer compensation — humidity */
static uint32_t BME280_CompensateHumidity(int32_t adc_H)
{
    int32_t v;
    v = t_fine - 76800;
    v = (((((adc_H << 14) - ((int32_t)dig_H4 << 20)
          - ((int32_t)dig_H5 * v)) + 16384) >> 15)
       * (((((((v * (int32_t)dig_H6) >> 10)
             * (((v * (int32_t)dig_H3) >> 11) + 32768)) >> 10)
            + 2097152) * (int32_t)dig_H2 + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)dig_H1) >> 4);
    if (v < 0) v = 0;
    if (v > 419430400) v = 419430400;
    return (uint32_t)(v >> 12); /* Q22.10 → divide by 1024 for %RH */
}

static void BME280_ReadAll_SPI(void)
{
    uint8_t raw[8];
    BME280_SPI_ReadRegs(BME280_REG_PRESS_MSB, raw, 8);

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    int32_t adc_H = ((int32_t)raw[6] << 8)  |  raw[7];

    int32_t  comp_T = BME280_CompensateTemp(adc_T);
    uint32_t comp_P = BME280_CompensatePressure(adc_P);
    uint32_t comp_H = BME280_CompensateHumidity(adc_H);

    temp_C   = comp_T / 100.0f;
    temp_F   = temp_C * 9.0f / 5.0f + 32.0f;
    pres_hPa = (comp_P / 256.0f) / 100.0f;
    hum_RH   = comp_H / 1024.0f;
}

/* ==================================================================
   UART Sensor Data Output
   ================================================================== */
static void UART_SendSensorData(void)
{
    char msg[128];
    sprintf(msg,
        "[SPI] Temp:%.2fC/%.2fF Pres:%.2fhPa Hum:%.2f%%\r\n",
        temp_C, temp_F, pres_hPa, hum_RH);
    UART_SendString(msg);
}

/* ==================================================================
   Verification Tests — Part A
   ================================================================== */
static void Test_A4_Plausibility(void)
{
    if (temp_C < 15.0f || temp_C > 40.0f)
        UART_SendString("[A4] Temp FAIL\r\n");
    else if (pres_hPa < 900 || pres_hPa > 1100)
        UART_SendString("[A4] Pres FAIL\r\n");
    else if (hum_RH < 0 || hum_RH > 100)
        UART_SendString("[A4] Hum  FAIL\r\n");
    else
        UART_SendString("[A4] Plausibility PASS\r\n");
}

/* ==================================================================
   TIM6 ISR — fires every 1 second
   ================================================================== */
static volatile uint32_t ticks = 0;

void TIM6_DAC_IRQHandler(void)
{
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR &= ~TIM_SR_UIF;   /* clear flag */

        ticks++;

        /* Test A3 — heartbeat */
        {
            char s[32];
            sprintf(s, "[A3] Tick:%lu\r\n", ticks);
            UART_SendString(s);
        }

        BME280_ReadAll_SPI();
        UART_SendSensorData();
        Test_A4_Plausibility();
    }
}

/* ==================================================================
   main
   ================================================================== */
int main(void)
{
    SystemClock_Config();
    GPIO_Config();
    USART2_Config();

    /* ---- Test A2: UART loopback (before BME280 init) ---- */
    UART_SendString("[A2] UART OK\r\n");

    /* Banner */
    UART_SendString("========================================\r\n");
    UART_SendString("BME280 via SPI -- CSE 2206 Lab A\r\n");
    UART_SendString("========================================\r\n");

    SPI2_Config();
    BME280_Init_SPI();   /* also prints [A1] ChipID */
    TIM6_Config();

    while (1) {
        /* All work done in TIM6 ISR */
    }
}
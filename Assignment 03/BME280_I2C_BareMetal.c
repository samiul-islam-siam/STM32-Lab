/*
 * CSE 2206 - Microcontroller and Embedded System
 * Lab Assignment 3 - Part B: BME280 via I2C (Bare-Metal / Register-Level)
 * Platform : STM32F446RE Nucleo-64
 * UART     : USART2 @ 115200 baud (PA2=TX, PA3=RX)
 * I2C      : I2C1  (PB6=SCL, PB7=SDA)
 * Timer    : TIM6  @ 1-second interrupt
 *
 * Hardware note: 4.7 kΩ pull-up resistors required on PB6 and PB7 to 3.3 V
 *                (most breakout boards include them).
 */

#include "stm32f446xx.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  BME280 I2C Address (SDO → GND)                                    */
/* ------------------------------------------------------------------ */
#define BME280_I2C_ADDR   0x76   /* 7-bit address */

/* ------------------------------------------------------------------ */
/*  BME280 Register Map                                                */
/* ------------------------------------------------------------------ */
#define BME280_REG_CHIP_ID    0xD0
#define BME280_REG_RESET      0xE0
#define BME280_REG_CTRL_HUM   0xF2
#define BME280_REG_CTRL_MEAS  0xF4
#define BME280_REG_CONFIG     0xF5
#define BME280_REG_PRESS_MSB  0xF7

/* ------------------------------------------------------------------ */
/*  Calibration Storage                                                */
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

static int32_t t_fine;
static float   temp_C, temp_F, pres_hPa, hum_RH;

/* ------------------------------------------------------------------ */
/*  Simple delay                                                       */
/* ------------------------------------------------------------------ */
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 18000; i++) {
        __asm("NOP");
    }
}

/* ==================================================================
   STEP A3.1 — Clock Configuration (180 MHz) — same as Part A
   ================================================================== */
static void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    RCC->PLLCFGR = (4U  << RCC_PLLCFGR_PLLM_Pos)
                 | (180U << RCC_PLLCFGR_PLLN_Pos)
                 | (0U   << RCC_PLLCFGR_PLLP_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSE;

    FLASH->ACR = FLASH_ACR_LATENCY_5WS
               | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN
               | FLASH_ACR_DCEN;

    RCC->CFGR = RCC_CFGR_HPRE_DIV1
              | RCC_CFGR_PPRE1_DIV4
              | RCC_CFGR_PPRE2_DIV2;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/* ==================================================================
   STEP A3.2 — UART GPIO (PA2/PA3) — same as Part A
   ================================================================== */
static void UART_GPIO_Config(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA2 — USART2 TX — AF7 */
    GPIOA->MODER   |= (2U << (2*2));
    GPIOA->AFR[0]  |= (7U << (2*4));
    GPIOA->OSPEEDR |= (3U << (2*2));

    /* PA3 — USART2 RX — AF7 */
    GPIOA->MODER   |= (2U << (3*2));
    GPIOA->AFR[0]  |= (7U << (3*4));
    GPIOA->OSPEEDR |= (3U << (3*2));
}

/* ==================================================================
   STEP B3.1 — I2C1 GPIO Configuration (PB6=SCL, PB7=SDA)
   ================================================================== */
static void I2C1_GPIO_Config(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PB6 — SCL — AF4, open-drain, medium speed, pull-up */
    GPIOB->MODER   |=  (2U << (6*2));   /* Alternate Function  */
    GPIOB->AFR[0]  |=  (4U << (6*4));   /* AF4 = I2C1          */
    GPIOB->OTYPER  |=  (1U << 6);       /* Open-drain          */
    GPIOB->OSPEEDR |=  (1U << (6*2));   /* Medium speed (01)   */
    GPIOB->PUPDR   |=  (1U << (6*2));   /* Pull-up             */

    /* PB7 — SDA — AF4, open-drain, medium speed, pull-up */
    GPIOB->MODER   |=  (2U << (7*2));
    GPIOB->AFR[0]  |=  (4U << (7*4));
    GPIOB->OTYPER  |=  (1U << 7);
    GPIOB->OSPEEDR |=  (1U << (7*2));
    GPIOB->PUPDR   |=  (1U << (7*2));
}

/* ==================================================================
   STEP A3.3 — USART2 Configuration
   ================================================================== */
static void USART2_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    USART2->BRR   = (24U << 4) | 7U;   /* 115200 @ 45 MHz APB1 */
    USART2->CR1   = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void UART_SendString(const char *s)
{
    while (*s) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = (uint8_t)(*s++);
    }
}

/* ==================================================================
   STEP A3.4 — TIM6 (1-second interrupt)
   ================================================================== */
static void TIM6_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6->PSC = 9000 - 1;    /* 90 MHz / 9000 → 10 kHz  */
    TIM6->ARR = 10000 - 1;   /* 10 kHz / 10000 → 1 Hz   */
    TIM6->DIER |= TIM_DIER_UIE;
    TIM6->CR1  |= TIM_CR1_CEN;
    NVIC_SetPriority(TIM6_DAC_IRQn, 2);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/* ==================================================================
   STEP B3.2 — I2C1 Peripheral Configuration (100 kHz, APB1 = 45 MHz)
   ================================================================== */
static void I2C1_Config(void)
{
    /* Enable I2C1 clock */
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* Reset I2C1 to clear any previous state */
    RCC->APB1RSTR |=  RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    /* CR2: FREQ = APB1 clock in MHz = 45 */
    I2C1->CR2 = 45U;

    /*
     * CCR: Standard mode (Sm), 100 kHz
     * T_high = 5 µs; CCR = 5000 ns / (1000/45 ns) = 225
     */
    I2C1->CCR = 225U;

    /*
     * TRISE: max rise time in Sm = 1000 ns
     * TRISE = floor(1000 ns / T_pclk1) + 1 = 45 + 1 = 46
     */
    I2C1->TRISE = 46U;

    /* Enable I2C1: CR1 PE bit */
    I2C1->CR1 |= I2C_CR1_PE;
}

/* ==================================================================
   I2C Helper Functions (bare-metal)
   ================================================================== */

/* B4.1 — Write a single register */
static void I2C_WriteReg(uint8_t reg, uint8_t data)
{
    /* Generate START */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    /* Send slave address + WRITE (bit0 = 0) */
    I2C1->DR = (BME280_I2C_ADDR << 1) | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;   /* clear ADDR flag */

    /* Send register address */
    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_TXE));

    /* Send data byte */
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    /* Generate STOP */
    I2C1->CR1 |= I2C_CR1_STOP;
}

/* B4.2 — Burst read N registers */
static void I2C_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    /* --- Phase 1: write register pointer --- */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (BME280_I2C_ADDR << 1) | 0;   /* addr + WRITE */
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    /* --- Phase 2: repeated START + READ --- */
    I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (BME280_I2C_ADDR << 1) | 1;   /* addr + READ */
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    for (uint8_t i = 0; i < len; i++) {
        if (i == len - 1)
            I2C1->CR1 &= ~I2C_CR1_ACK;   /* NACK on last byte */
        while (!(I2C1->SR1 & I2C_SR1_RXNE));
        buf[i] = (uint8_t)I2C1->DR;
    }

    I2C1->CR1 |= I2C_CR1_STOP;
}

/* ==================================================================
   Calibration Read (I2C)
   ================================================================== */
static void BME280_ReadCalibration(void)
{
    uint8_t buf[26];

    I2C_ReadRegs(0x88, buf, 24);
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

    I2C_ReadRegs(0xA1, buf, 1);
    dig_H1 = buf[0];

    I2C_ReadRegs(0xE1, buf, 7);
    dig_H2 = (int16_t)(buf[1] << 8) | buf[0];
    dig_H3 = buf[2];
    dig_H4 = (int16_t)(buf[3] << 4) | (buf[4] & 0x0F);
    dig_H5 = (int16_t)(buf[5] << 4) | (buf[4] >> 4);
    dig_H6 = (int8_t) buf[6];
}

/* ==================================================================
   STEP B3.3 — BME280 Init (I2C)
   ================================================================== */
static void BME280_Init_I2C(void)
{
    /* Soft reset */
    I2C_WriteReg(BME280_REG_RESET, 0xB6);
    delay_ms(10);

    /* Chip ID */
    uint8_t id;
    I2C_ReadRegs(BME280_REG_CHIP_ID, &id, 1);
    {
        char s[64];
        sprintf(s, "[B1] ChipID=0x%02X (expect 0x60)\r\n", id);
        UART_SendString(s);
    }

    /* Test B2: ACK check */
    {
        I2C1->CR1 |= I2C_CR1_START;
        while (!(I2C1->SR1 & I2C_SR1_SB));
        I2C1->DR = (BME280_I2C_ADDR << 1) | 0;

        uint32_t timeout = 100000;
        while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);

        if (!timeout) {
            UART_SendString("[B2] I2C ACK FAIL\r\n");
        } else {
            (void)I2C1->SR2;
            UART_SendString("[B2] I2C ACK OK\r\n");
        }
        I2C1->CR1 |= I2C_CR1_STOP;
        delay_ms(2);
    }

    BME280_ReadCalibration();

    I2C_WriteReg(BME280_REG_CTRL_HUM,  0x01); /* osrs_h = x1      */
    I2C_WriteReg(BME280_REG_CTRL_MEAS, 0x57); /* osrs_t/p, normal */
    I2C_WriteReg(BME280_REG_CONFIG,    0x10); /* filter=16, 0.5ms */

    delay_ms(100);
}

/* ==================================================================
   Compensation Formulas (identical to Part A)
   ================================================================== */
static int32_t BME280_CompensateTemp(int32_t adc_T)
{
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1)))
            * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1))
            * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
            * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

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
    return (uint32_t)p;
}

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
    return (uint32_t)(v >> 12);
}

/* ==================================================================
   STEP B3.4 — Read All + Compensate (I2C)
   ================================================================== */
static void BME280_ReadAll_I2C(void)
{
    uint8_t raw[8];
    I2C_ReadRegs(BME280_REG_PRESS_MSB, raw, 8);

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
   UART Output — B4.3
   ================================================================== */
static void UART_SendSensorData(void)
{
    char msg[128];
    sprintf(msg,
        "[I2C] Temp:%.2fC/%.2fF Pres:%.2fhPa Hum:%.2f%%\r\n",
        temp_C, temp_F, pres_hPa, hum_RH);
    UART_SendString(msg);
}

/* ==================================================================
   TIM6 ISR — fires every 1 second
   ================================================================== */
static volatile uint32_t ticks = 0;

void TIM6_DAC_IRQHandler(void)
{
    if (TIM6->SR & TIM_SR_UIF) {
        TIM6->SR &= ~TIM_SR_UIF;

        ticks++;

        /* Test B3 — heartbeat */
        {
            char s[32];
            sprintf(s, "[B3] Tick:%lu\r\n", ticks);
            UART_SendString(s);
        }

        BME280_ReadAll_I2C();
        UART_SendSensorData();

        /* Test B4 — plausibility */
        if (temp_C < 15.0f || temp_C > 40.0f
         || pres_hPa < 900 || pres_hPa > 1100
         || hum_RH < 0 || hum_RH > 100)
            UART_SendString("[B4] Plausibility FAIL\r\n");
        else
            UART_SendString("[B4] Plausibility PASS\r\n");
    }
}

/* ==================================================================
   main
   ================================================================== */
int main(void)
{
    SystemClock_Config();
    UART_GPIO_Config();
    I2C1_GPIO_Config();
    USART2_Config();

    /* Test B2 (UART alive) — print before BME280 init */
    UART_SendString("[B2-UART] UART OK\r\n");

    /* Banner */
    UART_SendString("========================================\r\n");
    UART_SendString("BME280 via I2C -- CSE 2206 Lab B\r\n");
    UART_SendString("========================================\r\n");

    I2C1_Config();
    BME280_Init_I2C();   /* prints [B1] ChipID and [B2] ACK */
    TIM6_Config();

    while (1) {
        /* All work done in TIM6 ISR */
    }
}
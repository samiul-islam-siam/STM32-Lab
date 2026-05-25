/*
 * BMP280 — Temperature & Pressure Read via I2C
 * Author: Siam & Partho
 *
 * Wiring:
 *   BMP280 VCC  -> 3.3V
 *   BMP280 GND  -> GND
 *   BMP280 SCL  -> PB6
 *   BMP280 SDA  -> PB7
 *   BMP280 SDO  -> GND
 *   BMP280 CSB  -> 3.3V
 */

#include "stm32f446xx.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  BMP280 I2C Address & Register Map                                 */
/* ------------------------------------------------------------------ */
#define BMP280_I2C_ADDR         0x76
#define BMP280_REG_CHIP_ID      0xD0
#define BMP280_REG_RESET        0xE0
#define BMP280_REG_STATUS       0xF3
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_CALIB_START  0x88
#define CHIP_ID_BME280          0x60
#define CHIP_ID_BMP280          0x58
#define BMP280_RESET_VALUE      0xB6
#define BMP280_CTRL_MEAS_VAL    ((2U << 5) | (5U << 2) | 3U)   /* 0x57 */
#define BMP280_CONFIG_VAL       ((4U << 5) | (4U << 2))        /* 0x90 */

/* ------------------------------------------------------------------ */
/*  Calibration data struct (from registers 0x88..0x9F)               */
/* ------------------------------------------------------------------ */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} BMP280_Calib_t;

static BMP280_Calib_t calib;
static int32_t t_fine;

/* ------------------------------------------------------------------ */
/*  Simple delay (~1 ms per unit at 180 MHz)                          */
/* ------------------------------------------------------------------ */
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 18000; i++) {
        __asm("NOP");
    }
}

static void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    RCC->PLLCFGR = (4U   << RCC_PLLCFGR_PLLM_Pos)
                 | (180U << RCC_PLLCFGR_PLLN_Pos)
                 | (0U   << RCC_PLLCFGR_PLLP_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSE;

    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    RCC->CFGR = RCC_CFGR_HPRE_DIV1
              | RCC_CFGR_PPRE1_DIV4
              | RCC_CFGR_PPRE2_DIV2;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

static void UART_Config(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER   |= (2U << (2*2));
    GPIOA->AFR[0]  |= (7U << (2*4));
    GPIOA->OSPEEDR |= (3U << (2*2));

    USART2->BRR = (24U << 4) | 7U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void UART_Print(const char *s)
{
    while (*s) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = (uint8_t)(*s++);
    }
}

/* ================================================================== */
/*  I2C CONFIG: PB6=SCL, PB7=SDA, AF4, open-drain, 100 kHz            */
/* ================================================================== */
static void I2C_Config(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* ---- PB6: SCL, AF4, open-drain ---- */
    GPIOB->MODER   |=  (2U << (6 * 2));    /* AF mode */
    GPIOB->AFR[0]  |=  (4U << (6 * 4));    /* AF4 = I2C1_SCL */
    GPIOB->OTYPER  |=  (1U << 6);          /* Open-drain */
    GPIOB->OSPEEDR |=  (1U << (6 * 2));    /* Medium speed */
    GPIOB->PUPDR   |=  (1U << (6 * 2));    /* Pull-up */

    /* ---- PB7: SDA, AF4, open-drain ---- */
    GPIOB->MODER   |=  (2U << (7 * 2));    /* AF mode */
    GPIOB->AFR[0]  |=  (4U << (7 * 4));    /* AFR[1], index 1 = PB9 */
    GPIOB->OTYPER  |=  (1U << 7);          /* Open-drain */
    GPIOB->OSPEEDR |=  (1U << (7 * 2));    /* Medium speed */
    GPIOB->PUPDR   |=  (1U << (7 * 2));    /* Pull-up */

    /* ---- I2C1 peripheral ---- */
    RCC->APB1ENR  |=  RCC_APB1ENR_I2C1EN;
    RCC->APB1RSTR |=  RCC_APB1RSTR_I2C1RST;    /* Reset */
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    I2C1->CR2   = 45U;      /* APB1 = 45 MHz */
    I2C1->CCR   = 225U;     /* Standard mode, 100 kHz */
    I2C1->TRISE = 46U;
    I2C1->CR1  |= I2C_CR1_PE;
}

/* ================================================================== */
/*  I2C Low-Level: Write one byte to a register                       */
/* ================================================================== */
static void I2C_WriteByte(uint8_t reg, uint8_t data)
{
    /* Generate START */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    /* Send address + WRITE */
    I2C1->DR = (BMP280_I2C_ADDR << 1) | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;    /* Clear ADDR flag */

    /* Send register */
    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_TXE));

    /* Send data */
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    /* Generate STOP */
    I2C1->CR1 |= I2C_CR1_STOP;
}

/* ================================================================== */
/*  I2C Low-Level: Read one byte from a register                      */
/* ================================================================== */
static uint8_t I2C_ReadByte(uint8_t reg)
{
    uint8_t val = 0xFF;

    /* Write phase: send register address */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (BMP280_I2C_ADDR << 1) | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    /* Repeated START for read phase */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    /* NACK immediately (only 1 byte coming) */
    I2C1->CR1 &= ~I2C_CR1_ACK;
    I2C1->DR = (BMP280_I2C_ADDR << 1) | 1;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    /* Queue STOP before reading last byte (STM32 requirement) */
    I2C1->CR1 |= I2C_CR1_STOP;

    while (!(I2C1->SR1 & I2C_SR1_RXNE));
    val = (uint8_t)I2C1->DR;

    return val;
}

/* ================================================================== */
/*  I2C Low-Level: Burst read N bytes starting at reg                 */
/* ================================================================== */
static void I2C_ReadBytes(uint8_t start_reg, uint8_t *buf, uint8_t len)
{
    if (len == 0) return;

    /* Write phase: send starting register address */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (BMP280_I2C_ADDR << 1) | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    I2C1->DR = start_reg;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    /* Repeated START for read phase */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    /* Enable ACK for multi-byte reads */
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->DR = (BMP280_I2C_ADDR << 1) | 1;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    for (uint8_t i = 0; i < len; i++) {
        if (i == (len - 1)) {
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |=  I2C_CR1_STOP;
        }
        while (!(I2C1->SR1 & I2C_SR1_RXNE));
        buf[i] = (uint8_t)I2C1->DR;
    }
}

static int I2C_DevicePresent(void)
{
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (BMP280_I2C_ADDR << 1) | 0;

    uint32_t timeout = 200000;
    while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF)) && --timeout);

    int present = (I2C1->SR1 & I2C_SR1_ADDR) != 0;

    if (present)
        (void)I2C1->SR2;        /* Clear ADDR flag */
    else
        I2C1->SR1 &= ~I2C_SR1_AF; /* Clear AF flag */

    I2C1->CR1 |= I2C_CR1_STOP;
    delay_ms(2);
    return present;
}

/* ================================================================== */
/*  BMP280: Read 24 bytes of factory calibration from 0x88..0x9F      */
/* ================================================================== */
static void BMP280_ReadCalib(void)
{
    uint8_t raw[24];
    I2C_ReadBytes(BMP280_REG_CALIB_START, raw, 24);

    calib.dig_T1 = (uint16_t)((raw[1]  << 8) | raw[0]);
    calib.dig_T2 = (int16_t) ((raw[3]  << 8) | raw[2]);
    calib.dig_T3 = (int16_t) ((raw[5]  << 8) | raw[4]);

    calib.dig_P1 = (uint16_t)((raw[7]  << 8) | raw[6]);
    calib.dig_P2 = (int16_t) ((raw[9]  << 8) | raw[8]);
    calib.dig_P3 = (int16_t) ((raw[11] << 8) | raw[10]);
    calib.dig_P4 = (int16_t) ((raw[13] << 8) | raw[12]);
    calib.dig_P5 = (int16_t) ((raw[15] << 8) | raw[14]);
    calib.dig_P6 = (int16_t) ((raw[17] << 8) | raw[16]);
    calib.dig_P7 = (int16_t) ((raw[19] << 8) | raw[18]);
    calib.dig_P8 = (int16_t) ((raw[21] << 8) | raw[20]);
    calib.dig_P9 = (int16_t) ((raw[23] << 8) | raw[22]);
}

/* ================================================================== */
/*  BMP280: Configure oversampling and Normal mode                    */
/* ================================================================== */
static void BMP280_Init(void)
{
    /* Soft-reset first to clear any previous state */
    I2C_WriteByte(BMP280_REG_RESET, BMP280_RESET_VALUE);
    delay_ms(10);

    /* config: IIR filter x16, standby 500 ms */
    I2C_WriteByte(BMP280_REG_CONFIG, BMP280_CONFIG_VAL);
    delay_ms(2);

    /* ctrl_meas: temp x2, pressure x16, Normal mode */
    I2C_WriteByte(BMP280_REG_CTRL_MEAS, BMP280_CTRL_MEAS_VAL);
    delay_ms(2);
}

/* ================================================================== */
/*  Bosch Compensation: Temperature                                   */
/* ================================================================== */
static int32_t BMP280_CompensateTemp(int32_t adc_T)
{
    int32_t var1, var2, T;

    var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1)))
             * ((int32_t)calib.dig_T2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1))
             * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12)
             * ((int32_t)calib.dig_T3)) >> 14;

    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

/* ================================================================== */
/*  Bosch Compensation: Pressure                                      */
/* ================================================================== */
static uint32_t BMP280_CompensatePress(int32_t adc_P)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8)
         + ((var1 * (int64_t)calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;

    if (var1 == 0) {
        return 0;   /* avoid division by zero */
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;

    var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);

    return (uint32_t)(p >> 8);
}

/* ================================================================== */
/*  BMP280: Read raw ADC data and print compensated values            */
/* ================================================================== */
static void BMP280_ReadAndPrint(void)
{
    char msg[80];
    uint8_t raw[6];

    I2C_ReadBytes(BMP280_REG_PRESS_MSB, raw, 6);

    int32_t adc_P = ((int32_t)raw[0] << 12)
                  | ((int32_t)raw[1] <<  4)
                  | ((int32_t)raw[2] >>  4);

    int32_t adc_T = ((int32_t)raw[3] << 12)
                  | ((int32_t)raw[4] <<  4)
                  | ((int32_t)raw[5] >>  4);

    int32_t  temp_raw  = BMP280_CompensateTemp(adc_T);
    uint32_t press_pa  = BMP280_CompensatePress(adc_P);

    /* Format temperature: temp_raw is in 0.01 degC units */
    int32_t temp_int  =  temp_raw / 100;
    int32_t temp_frac = (temp_raw < 0 ? -temp_raw : temp_raw) % 100;

    /* Format pressure: press_pa is in Pa, convert to hPa */
    uint32_t press_hpa_int  = press_pa / 100;
    uint32_t press_hpa_frac = press_pa % 100;

    sprintf(msg, "  Temperature : %ld.%02ld C\r\n",
            (long)temp_int, (long)temp_frac);
    UART_Print(msg);

    sprintf(msg, "  Pressure    : %lu.%02lu hPa\r\n",
            (unsigned long)press_hpa_int, (unsigned long)press_hpa_frac);
    UART_Print(msg);

    UART_Print("  ----------------------------------------\r\n");
}

/* ================================================================== */
/*  Sensor Detection                                                  */
/* ================================================================== */
static void Detect_Sensor(void)
{
    char msg[80];

    UART_Print("========================================\r\n");
    UART_Print("  BME/P280 Sensor on STM32F446RE\r\n");
    UART_Print("========================================\r\n");

    UART_Print("\r\n[1] Probing I2C address 0x76 ...\r\n");
    if (!I2C_DevicePresent()) {
        UART_Print("    FAIL: No ACK received.\r\n");
        UART_Print("    Check wiring and pull-up resistors.\r\n");
        UART_Print("    If SDO is tied to VCC, try address 0x77.\r\n");
        while (1); /* halt */
    }
    UART_Print("    OK: Device ACK received.\r\n");

    UART_Print("\r\n[2] Reading chip ID register (0xD0) ...\r\n");
    uint8_t chip_id = I2C_ReadByte(BMP280_REG_CHIP_ID);
    sprintf(msg, "    Raw value: 0x%02X\r\n", chip_id);
    UART_Print(msg);

    UART_Print("\r\n[3] Identifying sensor ...\r\n");
    if (chip_id == CHIP_ID_BME280) {
        UART_Print("    >>> BME280 detected (has humidity) <<<\r\n");
        UART_Print("    Note: humidity not read in this firmware.\r\n");
    } else if (chip_id == CHIP_ID_BMP280) {
        UART_Print("    >>> BMP280 detected <<<\r\n");
        UART_Print("    Sensors: Temperature + Pressure\r\n");
    } else {
        sprintf(msg, "    >>> UNKNOWN chip ID: 0x%02X <<<\r\n", chip_id);
        UART_Print(msg);
        UART_Print("    Halting. Check wiring or sensor.\r\n");
        while (1); /* halt */
    }

    UART_Print("========================================\r\n");
}

int main(void)
{
    SystemClock_Config();   /* 180 MHz */
    UART_Config();          /* USART2 @ 115200, PA2=TX */
    I2C_Config();           /* I2C1 @ 100 kHz, PB6=SCL, PB9=SDA */

    delay_ms(100);          /* Let BMP280 power up (needs ~2 ms) */

    /* Step 1: Detect and identify sensor */
    Detect_Sensor();

    /* Step 2: Read factory calibration coefficients */
    UART_Print("\r\n[4] Reading calibration data ...\r\n");
    BMP280_ReadCalib();
    UART_Print("    Calibration loaded.\r\n");

    /* Step 3: Configure oversampling and start Normal mode */
    UART_Print("\r\n[5] Configuring BMP280 (Normal mode, IIR x16) ...\r\n");
    BMP280_Init();
    UART_Print("    Ready.\r\n");
    delay_ms(100);

    /* Step 4: Print readings every second */
    UART_Print("\r\n--- Live Readings (1 Hz) ---\r\n");
    UART_Print("  ----------------------------------------\r\n");

    while (1) {
        BMP280_ReadAndPrint();
        delay_ms(1000);
    }
}
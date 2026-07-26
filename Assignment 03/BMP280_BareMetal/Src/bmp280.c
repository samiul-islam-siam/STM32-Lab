/*
 * BMP280.c  —  Bare-metal driver for STM32F446RE
 *
 * Author: Md. Samiul Islam Siam (02)
 *         Partho Kumar Mondal (07)
 */

#include "BMP280.h"
#include "helper.h"
#include <math.h>

/* ================================================================== */
/*  Low-level peripheral initializers (private)                       */
/* ================================================================== */

/**
 * @brief  Configure SPI2 (master, 4-wire, CPOL=0 CPHA=0, ~5.6 MHz).
 *
 *  Pin mapping
 *  ───────────
 *  PA9  → CS   (GPIO output, push-pull)
 *  PB10 → SCK  (AF5, SPI2_SCK)
 *  PC2  → MISO (AF5, SPI2_MISO)
 *  PC1  → MOSI (AF7, SPI2_MOSI)
 */
static void spi_init(void)
{
    /* ---- Enable clocks ---- */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN    /* PB (SCK)  */
                 |  RCC_AHB1ENR_GPIOCEN;   /* PC (MISO, MOSI) */
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    /* ---- PB9 — CS: output, push-pull, high speed, no pull ---- */
    GPIOB->MODER   &= ~(3U << (9U * 2U));
    GPIOB->MODER   |=  (1U << (9U * 2U));   /* output     */
    GPIOB->OTYPER  &= ~(1U << 9U);          /* push-pull  */
    GPIOB->OSPEEDR |=  (3U << (9U * 2U));   /* high speed */
    GPIOB->PUPDR   &= ~(3U << (9U * 2U));   /* no pull    */
    BMP280_CS_HIGH();                       /* idle HIGH  */

    /* ---- PB10 — SCK: AF5, push-pull, high speed ---- */
    GPIOB->MODER   &= ~(3U  << (10U * 2U));
    GPIOB->MODER   |=  (2U  << (10U * 2U));            /* AF */
    GPIOB->OSPEEDR |=  (3U  << (10U * 2U));
    GPIOB->AFR[1]  &= ~(0xFU << ((10U - 8U) * 4U));
    GPIOB->AFR[1]  |=  (5U   << ((10U - 8U) * 4U));   /* AF5 */

    /* ---- PC2 — SDL: AF5, push-pull, high speed ---- */
    GPIOC->MODER   &= ~(3U  << (2U * 2U));
    GPIOC->MODER   |=  (2U  << (2U * 2U));
    GPIOC->OSPEEDR |=  (3U  << (2U * 2U));
    GPIOC->AFR[0]  &= ~(0xFU << (2U * 4U));
    GPIOC->AFR[0]  |=  (5U   << (2U * 4U));            /* AF5 */

    /* ---- PC1 — SDA: AF7, push-pull, high speed ---- */
    GPIOC->MODER   &= ~(3U  << (1U * 2U));
    GPIOC->MODER   |=  (2U  << (1U * 2U));
    GPIOC->OSPEEDR |=  (3U  << (1U * 2U));
    GPIOC->AFR[0]  &= ~(0xFU << (1U * 4U));
    GPIOC->AFR[0]  |=  (7U   << (1U * 4U));            /* AF7 */

    /* ---- SPI2: master, software NSS, CPOL=0, CPHA=0, fPCLK1/8 ---- */
    SPI2->CR1 = 0;
    SPI2->CR1 = SPI_CR1_MSTR
              | SPI_CR1_SSM
              | SPI_CR1_SSI
              | (2U << SPI_CR1_BR_Pos)   /* APB1(45 MHz) / 8 ≈ 5.6 MHz */
              | SPI_CR1_SPE;
}

/**
 * @brief  Configure I2C1 at 100 kHz (PB6 = SCL, PB7 = SDA, AF4).
 */
static void i2c_init(void)
{
    /* ---- GPIO clock ---- */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PB6 SCL — AF4, open-drain, medium speed, pull-up */
    GPIOB->MODER   &= ~(3U << (6U * 2U));
    GPIOB->MODER   |=  (2U << (6U * 2U));
    GPIOB->AFR[0]  &= ~(0xFU << (6U * 4U));
    GPIOB->AFR[0]  |=  (4U   << (6U * 4U));
    GPIOB->OTYPER  |=  (1U << 6U);
    GPIOB->OSPEEDR |=  (1U << (6U * 2U));
    GPIOB->PUPDR   &= ~(3U << (6U * 2U));
    GPIOB->PUPDR   |=  (1U << (6U * 2U));

    /* PB7 SDA — AF4, open-drain, medium speed, pull-up */
    GPIOB->MODER   &= ~(3U << (7U * 2U));
    GPIOB->MODER   |=  (2U << (7U * 2U));
    GPIOB->AFR[0]  &= ~(0xFU << (7U * 4U));
    GPIOB->AFR[0]  |=  (4U   << (7U * 4U));
    GPIOB->OTYPER  |=  (1U << 7U);
    GPIOB->OSPEEDR |=  (1U << (7U * 2U));
    GPIOB->PUPDR   &= ~(3U << (7U * 2U));
    GPIOB->PUPDR   |=  (1U << (7U * 2U));

    /* ---- I2C1 peripheral ---- */
    RCC->APB1ENR  |=  RCC_APB1ENR_I2C1EN;
    RCC->APB1RSTR |=  RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    /*
     * Standard mode, APB1 = 45 MHz:
     *   FREQ  = 45
     *   CCR   = 45 000 000 / (2 × 100 000) = 225
     *   TRISE = (1 000 ns × 45 MHz) + 1 = 46
     */
    I2C1->CR2   = 45U;
    I2C1->CCR   = 225U;
    I2C1->TRISE = 46U;
    I2C1->CR1  |= I2C_CR1_PE;
}

/* ================================================================== */
/*  SPI primitives (private)                                          */
/* ================================================================== */

static uint8_t spi_transfer(uint8_t data)
{
    while (!(SPI2->SR & SPI_SR_TXE));
    SPI2->DR = data;
    while (!(SPI2->SR & SPI_SR_RXNE));
    return (uint8_t)SPI2->DR;
}

static uint8_t spi_read_byte(uint8_t reg)
{
    BMP280_CS_LOW();
    spi_transfer(reg | 0x80U);          /* bit7 = 1 → read */
    uint8_t val = spi_transfer(0xFFU);
    BMP280_CS_HIGH();
    return val;
}

static void spi_write_byte(uint8_t reg, uint8_t data)
{
    BMP280_CS_LOW();
    spi_transfer(reg & 0x7FU);          /* bit7 = 0 → write */
    spi_transfer(data);
    BMP280_CS_HIGH();
}

static void spi_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    BMP280_CS_LOW();
    spi_transfer(reg | 0x80U);
    for (uint8_t i = 0; i < len; i++)
        buf[i] = spi_transfer(0xFFU);
    BMP280_CS_HIGH();
}

/* ================================================================== */
/*  I2C primitives (private)                                           */
/* ================================================================== */

static void i2c_write_byte(uint8_t reg, uint8_t data)
{
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (BMP280_I2C_ADDR << 1) | 0U;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;
}

/**
 * @brief  Burst-read @p len bytes from @p start_reg over I2C.
 *
 *  Handles single-byte and multi-byte cases with correct ACK/NACK/STOP
 *  sequencing as required by the STM32 I2C state machine.
 */
static void i2c_read_bytes(uint8_t start_reg, uint8_t *buf, uint8_t len)
{
    if (len == 0) return;

    /* ---- Write phase: send register pointer ---- */
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = (BMP280_I2C_ADDR << 1) | 0U;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    I2C1->DR = start_reg;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    /* ---- Repeated START — read phase ---- */
    if (len == 1) {
        /*
         * Single-byte receive: disable ACK before clearing ADDR
         * so the NACK is sent on the only byte.
         */
        I2C1->CR1 &= ~I2C_CR1_ACK;

        I2C1->CR1 |= I2C_CR1_START;
        while (!(I2C1->SR1 & I2C_SR1_SB));

        I2C1->DR = (BMP280_I2C_ADDR << 1) | 1U;
        while (!(I2C1->SR1 & I2C_SR1_ADDR));
        /* Clear ADDR then immediately set STOP */
        (void)I2C1->SR2;
        I2C1->CR1 |= I2C_CR1_STOP;

        while (!(I2C1->SR1 & I2C_SR1_RXNE));
        buf[0] = (uint8_t)I2C1->DR;
    }
    else {
        /* Multi-byte: ACK all bytes, NACK+STOP before last */
        I2C1->CR1 |= I2C_CR1_ACK;

        I2C1->CR1 |= I2C_CR1_START;
        while (!(I2C1->SR1 & I2C_SR1_SB));

        I2C1->DR = (BMP280_I2C_ADDR << 1) | 1U;
        while (!(I2C1->SR1 & I2C_SR1_ADDR));
        (void)I2C1->SR2;

        for (uint8_t i = 0; i < len; i++) {
            if (i == (len - 1)) {
                /* Last byte: clear ACK then STOP before reading DR */
                I2C1->CR1 &= ~I2C_CR1_ACK;
                I2C1->CR1 |=  I2C_CR1_STOP;
            }
            while (!(I2C1->SR1 & I2C_SR1_RXNE));
            buf[i] = (uint8_t)I2C1->DR;
        }
    }
}

/* ================================================================== */
/*  Internal helpers (private)                                        */
/* ================================================================== */

/**
 * @brief  Write one byte to a sensor register via the active bus.
 */
static bmp280_status_t bmp280_write_register(bmp280_t *bmp,
                                             uint8_t   reg,
                                             uint8_t   value)
{
    if (bmp->comm_mode == BMP280_MODE_I2C)
        i2c_write_byte(reg, value);
    else
        spi_write_byte(reg, value);

    return BMP280_OK;
}

/**
 * @brief  Read @p length consecutive bytes from @p reg via the active bus.
 */
static bmp280_status_t bmp280_read_registers(bmp280_t *bmp,
                                             uint8_t   reg,
                                             uint8_t  *buffer,
                                             uint16_t  length)
{
    if (bmp->comm_mode == BMP280_MODE_I2C)
        i2c_read_bytes(reg, buffer, (uint8_t)length);
    else
        spi_read_burst(reg, buffer, (uint8_t)length);

    return BMP280_OK;
}

/**
 * @brief  Poll status register bit 3 (measuring) until clear.
 * @return BMP280_OK on ready, BMP280_TIMEOUT after 300 ms.
 */
static bmp280_status_t bmp280_wait_ready(bmp280_t *bmp)
{
    uint8_t  status  = 0;
    uint32_t timeout = 300U;   /* 300 × 1 ms */

    do {
        delay_ms(1U);
        if (bmp280_read_registers(bmp, BMP280_REG_STATUS,
                                  &status, 1U) != BMP280_OK)
            return BMP280_ERROR;
        if (--timeout == 0U)
            return BMP280_TIMEOUT;
    } while (status & 0x08U);  /* bit 3 = measuring */

    return BMP280_OK;
}

/**
 * @brief  Read 24 bytes of factory calibration data (0x88–0x9F).
 */
static bmp280_status_t bmp280_read_calibration(bmp280_t *bmp)
{
    uint8_t c[24];

    if (bmp280_read_registers(bmp, BMP280_REG_CALIB_START,
                              c, 24U) != BMP280_OK)
        return BMP280_ERROR;

    /* All coefficients are stored little-endian (LSB first) */
    bmp->dig_T1 = (uint16_t)((c[1]  << 8) | c[0]);
    bmp->dig_T2 =  (int16_t)((c[3]  << 8) | c[2]);
    bmp->dig_T3 =  (int16_t)((c[5]  << 8) | c[4]);

    bmp->dig_P1 = (uint16_t)((c[7]  << 8) | c[6]);
    bmp->dig_P2 =  (int16_t)((c[9]  << 8) | c[8]);
    bmp->dig_P3 =  (int16_t)((c[11] << 8) | c[10]);
    bmp->dig_P4 =  (int16_t)((c[13] << 8) | c[12]);
    bmp->dig_P5 =  (int16_t)((c[15] << 8) | c[14]);
    bmp->dig_P6 =  (int16_t)((c[17] << 8) | c[16]);
    bmp->dig_P7 =  (int16_t)((c[19] << 8) | c[18]);
    bmp->dig_P8 =  (int16_t)((c[21] << 8) | c[20]);
    bmp->dig_P9 =  (int16_t)((c[23] << 8) | c[22]);

    return BMP280_OK;
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

void bmp280_comm_init(bmp280_t *bmp)
{
    if (bmp->comm_mode == BMP280_MODE_I2C)
        i2c_init();
    else
        spi_init();
}

bmp280_status_t bmp280_soft_reset(bmp280_t *bmp)
{
    bmp280_status_t s = bmp280_write_register(bmp,
                                              BMP280_REG_RESET,
                                              BMP280_SOFT_RESET_CMD);
    delay_ms(10U);   /* Wait for reset to complete */
    return s;
}

bmp280_status_t bmp280_init(bmp280_t *bmp)
{
    /* Soft-reset — guarantees a clean power-on state */
    if (bmp280_soft_reset(bmp) != BMP280_OK)
        return BMP280_ERROR;

    /* Verify chip ID */
    uint8_t chip_id = bmp280_read_chip_id(bmp);
    if (chip_id != CHIP_ID_BMP280 && chip_id != CHIP_ID_BME280)
        return BMP280_ERROR;

    /* Load calibration */
    if (bmp280_read_calibration(bmp) != BMP280_OK)
        return BMP280_ERROR;

    /* Apply default configuration: Normal mode, ×1 oversampling,
       filter off, 1 s standby — matches HAL version default          */
    return bmp280_set_configuration(bmp,
                                    BMP280_OPMODE_NORMAL,
                                    BMP280_OSAMPLE_1,
                                    BMP280_OSAMPLE_1,
                                    BMP280_FILTER_OFF,
                                    BMP280_STANDBY_1000_MS);
}

bmp280_status_t bmp280_set_configuration(bmp280_t               *bmp,
                                         bmp280_operating_mode_t mode,
                                         bmp280_oversampling_t   osrs_t,
                                         bmp280_oversampling_t   osrs_p,
                                         bmp280_filter_t         filter,
                                         bmp280_standby_time_t   standby)
{
    uint8_t ctrl_meas = 0U;
    uint8_t config    = 0U;

    ctrl_meas |= (uint8_t)(osrs_t << 5U);   /* Temperature oversampling */
    ctrl_meas |= (uint8_t)(osrs_p << 2U);   /* Pressure oversampling    */
    ctrl_meas |= (uint8_t) mode;            /* Operating mode           */

    config |= (uint8_t)(standby << 5U);     /* Standby time             */
    config |= (uint8_t)(filter  << 2U);     /* IIR filter               */

    if (bmp280_write_register(bmp, BMP280_REG_CTRL_MEAS, ctrl_meas) != BMP280_OK)
        return BMP280_ERROR;

    if (bmp280_write_register(bmp, BMP280_REG_CONFIG, config) != BMP280_OK)
        return BMP280_ERROR;

    return BMP280_OK;
}

bmp280_status_t bmp280_read_raw(bmp280_t *bmp,
                                int32_t  *temperature_raw,
                                int32_t  *pressure_raw)
{
    /* Wait until the sensor finishes its conversion */
    bmp280_status_t s = bmp280_wait_ready(bmp);
    if (s != BMP280_OK)
        return s;

    uint8_t data[6];
    if (bmp280_read_registers(bmp, BMP280_REG_PRESS_MSB,
                              data, 6U) != BMP280_OK)
        return BMP280_ERROR;

    *pressure_raw    = ((int32_t)data[0] << 12)
                     | ((int32_t)data[1] <<  4)
                     | ((int32_t)data[2] >>  4);

    *temperature_raw = ((int32_t)data[3] << 12)
                     | ((int32_t)data[4] <<  4)
                     | ((int32_t)data[5] >>  4);

    /* 0x80000 means the measurement was skipped (oversampling = 0x00) */
    if (*temperature_raw == 0x80000 || *pressure_raw == 0x80000)
        return BMP280_ERROR;

    return BMP280_OK;
}

float bmp280_compensate_temperature(bmp280_t *bmp, int32_t adc_T)
{
    float var1, var2;

    var1 = ((float)adc_T / 16384.0f - (float)bmp->dig_T1 / 1024.0f)
         * (float)bmp->dig_T2;

    var2 = ((float)adc_T / 131072.0f - (float)bmp->dig_T1 / 8192.0f)
         * ((float)adc_T / 131072.0f - (float)bmp->dig_T1 / 8192.0f)
         * (float)bmp->dig_T3;

    bmp->t_fine = var1 + var2;   /* stored per-instance for pressure use */
    return bmp->t_fine / 5120.0f;
}

float bmp280_compensate_pressure(bmp280_t *bmp, int32_t adc_P)
{
    float var1, var2, p;

    var1 = (bmp->t_fine / 2.0f) - 64000.0f;

    var2 = var1 * var1 * (float)bmp->dig_P6 / 32768.0f;
    var2 = var2 + var1 * (float)bmp->dig_P5 * 2.0f;
    var2 = var2 / 4.0f + (float)bmp->dig_P4 * 65536.0f;

    var1 = ((float)bmp->dig_P3 * var1 * var1 / 524288.0f
          + (float)bmp->dig_P2 * var1) / 524288.0f;
    var1 = (1.0f + var1 / 32768.0f) * (float)bmp->dig_P1;

    if (var1 == 0.0f)
        return 0.0f;   /* Guard against division by zero */

    p    = 1048576.0f - (float)adc_P;
    p    = ((p - var2 / 4096.0f) * 6250.0f) / var1;

    var1 = (float)bmp->dig_P9 * p * p / 2147483648.0f;
    var2 = p * (float)bmp->dig_P8 / 32768.0f;

    p    = p + (var1 + var2 + (float)bmp->dig_P7) / 16.0f;

    return p / 100.0f;   /* Convert Pa → hPa */
}

float bmp280_calculate_temperature(float temp_c)
{
    return (temp_c * 9.0f / 5.0f) + 32.0f;
}

float bmp280_calculate_altitude(float pressure, float sea_level_pressure)
{
    return 44330.0f * (1.0f - powf(pressure / sea_level_pressure, 0.1903f));
}

bmp280_status_t bmp280_read(bmp280_t *bmp, bmp280_data_t *out)
{
    int32_t adc_T, adc_P;

    bmp280_status_t s = bmp280_read_raw(bmp, &adc_T, &adc_P);
    if (s != BMP280_OK)
        return s;

    /* Temperature MUST be compensated first — it populates t_fine */
    out->temperature_c = bmp280_compensate_temperature(bmp, adc_T);
    out->temperature_f = bmp280_calculate_temperature(out->temperature_c);
    out->pressure_hpa  = bmp280_compensate_pressure(bmp, adc_P);
    out->altitude_m    = bmp280_calculate_altitude(out->pressure_hpa,
                                                   BMP280_SEA_LEVEL_HPA);
    return BMP280_OK;
}

uint8_t bmp280_read_chip_id(bmp280_t *bmp)
{
    uint8_t id = 0U;
    bmp280_read_registers(bmp, BMP280_REG_CHIP_ID, &id, 1U);
    return id;
}

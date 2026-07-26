/*
 * BMP280.c
 *
 * Author: Md. Samiul Islam Siam (02)
 *         Partho Kumar Mondal (07)
 */
#include "BMP280.h"
#include <string.h>
#include <math.h>

// BMP280 Register Addresses
#define BMP280_REG_CHIPID     0xD0
#define BMP280_REG_RESET      0xE0
#define BMP280_REG_STATUS     0xF3
#define BMP280_REG_CTRL_MEAS  0xF4
#define BMP280_REG_CONFIG     0xF5
#define BMP280_REG_PRESS_MSB  0xF7
#define BMP280_REG_PRESS_LSB  0xF8
#define BMP280_REG_PRESS_XLSB 0xF9
#define BMP280_REG_TEMP_MSB   0xFA
#define BMP280_REG_TEMP_LSB   0xFB
#define BMP280_REG_TEMP_XLSB  0xFC

// BMP280 Chip ID
#define BMP280_CHIPID         0x58

// BMP280 Reset Command
#define BMP280_SOFT_RESET_CMD 0xB6

// Helper Functions

static HAL_StatusTypeDef bmp280_write_register(bmp280_t *bmp, uint8_t reg, uint8_t value) {
    if (bmp->comm_mode == BMP280_MODE_I2C) {
        uint8_t data[2] = { reg, value };
        return HAL_I2C_Master_Transmit(bmp->hi2c,
                                       bmp->address << 1,
                                       data, 2,
                                       HAL_MAX_DELAY);
    }
    else { // SPI
        // clear bit7 to signal a write transaction
        uint8_t tx[2] = { reg & 0x7F, value };
        HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_RESET);
        HAL_StatusTypeDef status = HAL_SPI_Transmit(bmp->hspi,
                                                     tx, 2,
                                                     HAL_MAX_DELAY);
        HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_SET);
        return status;
    }
}

static HAL_StatusTypeDef bmp280_read_registers(bmp280_t *bmp, uint8_t reg, uint8_t *buffer, uint16_t length) {
    if (bmp->comm_mode == BMP280_MODE_I2C) {
        // Write register address
        if (HAL_I2C_Master_Transmit(bmp->hi2c, bmp->address << 1, &reg, 1, HAL_MAX_DELAY) != HAL_OK)
            return HAL_ERROR;
        // Read data
        return HAL_I2C_Master_Receive(bmp->hi2c, bmp->address << 1, buffer, length, HAL_MAX_DELAY);
    }
    else { // SPI
        // bit7 = 1 for read transaction
        uint8_t tx = reg | 0x80;
        HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_RESET);
        HAL_StatusTypeDef status = HAL_SPI_Transmit(bmp->hspi,
                                                     &tx, 1,
                                                     HAL_MAX_DELAY);
        if (status != HAL_OK) {
            HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_SET);
            return status;
        }
        status = HAL_SPI_Receive(bmp->hspi, buffer, length, HAL_MAX_DELAY);
        HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_SET);
        return status;
    }
}

// Status poll — waits until sensor is not measuring
static HAL_StatusTypeDef bmp280_wait_ready(bmp280_t *bmp)
{
    uint8_t  status  = 0;
    uint32_t timeout = 300;   /* 300 × 1 ms = 300 ms max */

    do {
        HAL_Delay(1);
        if (bmp280_read_registers(bmp, BMP280_REG_STATUS,
                                  &status, 1) != HAL_OK)
            return HAL_ERROR;
        if (--timeout == 0)
            return HAL_TIMEOUT;
    } while (status & 0x08);  /* bit3: measuring */

    return HAL_OK;
}

//  Calibration
static HAL_StatusTypeDef bmp280_read_calibration_data(bmp280_t *bmp)
{
    uint8_t c[24];

    // BMP280 has calibration registers from 0x88 to 0xA1
    // Read all calibration data
    if (bmp->comm_mode == BMP280_MODE_I2C) {
        if (HAL_I2C_Mem_Read(bmp->hi2c,
                             bmp->address << 1,
                             0x88,
                             I2C_MEMADD_SIZE_8BIT,
                             c, 24,
                             HAL_MAX_DELAY) != HAL_OK)
            return HAL_ERROR;
    }
    else { // SPI
        uint8_t tx = 0x88 | 0x80;
        HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_RESET);
        HAL_StatusTypeDef status = HAL_SPI_Transmit(bmp->hspi,
                                                     &tx, 1,
                                                     HAL_MAX_DELAY);
        if (status != HAL_OK) {
            HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_SET);
            return status;
        }
        status = HAL_SPI_Receive(bmp->hspi, c, 24, HAL_MAX_DELAY);
        HAL_GPIO_WritePin(bmp->cs_port, bmp->cs_pin, GPIO_PIN_SET);
        if (status != HAL_OK)
            return status;
    }

    bmp->dig_T1 = (uint16_t)(c[1]  << 8 | c[0]);
    bmp->dig_T2 =  (int16_t)(c[3]  << 8 | c[2]);
    bmp->dig_T3 =  (int16_t)(c[5]  << 8 | c[4]);

    bmp->dig_P1 = (uint16_t)(c[7]  << 8 | c[6]);
    bmp->dig_P2 =  (int16_t)(c[9]  << 8 | c[8]);
    bmp->dig_P3 =  (int16_t)(c[11] << 8 | c[10]);
    bmp->dig_P4 =  (int16_t)(c[13] << 8 | c[12]);
    bmp->dig_P5 =  (int16_t)(c[15] << 8 | c[14]);
    bmp->dig_P6 =  (int16_t)(c[17] << 8 | c[16]);
    bmp->dig_P7 =  (int16_t)(c[19] << 8 | c[18]);
    bmp->dig_P8 =  (int16_t)(c[21] << 8 | c[20]);
    bmp->dig_P9 =  (int16_t)(c[23] << 8 | c[22]);

    return HAL_OK;
}

//  Public APIs

// Soft Reset Function
HAL_StatusTypeDef bmp280_soft_reset(bmp280_t *bmp)
{
    HAL_StatusTypeDef s = bmp280_write_register(bmp, BMP280_REG_RESET, BMP280_SOFT_RESET_CMD);
    HAL_Delay(10); // Wait for reset to complete
    return s;
}

// Initialization Function
HAL_StatusTypeDef bmp280_init(bmp280_t *bmp) {
    HAL_StatusTypeDef status;

    // Verify communication mode
	if (bmp->comm_mode == BMP280_MODE_I2C && bmp->hi2c == NULL)
        return HAL_ERROR;

    if (bmp->comm_mode == BMP280_MODE_SPI && (bmp->hspi == NULL || bmp->cs_port == NULL))
        return HAL_ERROR;

    // soft reset first — guarantees clean power-on state
    if (bmp280_soft_reset(bmp) != HAL_OK)
        return HAL_ERROR;

    // Verify chip ID
    uint8_t chip_id = 0;
    if (bmp280_read_registers(bmp, BMP280_REG_CHIPID, &chip_id, 1) != HAL_OK)
        return HAL_ERROR;

    if (chip_id != BMP280_CHIPID)
        return HAL_ERROR;

    // Read calibration data
    if (bmp280_read_calibration_data(bmp) != HAL_OK)
        return HAL_ERROR;

    // Set default configuration: Normal mode, oversampling x1, filter off, standby time 1000ms
    status = bmp280_set_configuration(bmp,
                                    BMP280_MODE_NORMAL,
                                    BMP280_OSAMPLE_1,
                                    BMP280_OSAMPLE_1,
                                    BMP280_FILTER_OFF,
                                    BMP280_STANDBY_1000_MS);

    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

// Set Configuration Function
HAL_StatusTypeDef bmp280_set_configuration(bmp280_t *bmp,
                                           bmp280_operating_mode_t mode,
                                           bmp280_oversampling_t osrs_t,
                                           bmp280_oversampling_t osrs_p,
                                           bmp280_filter_t filter,
                                           bmp280_standby_time_t standby)
{
    uint8_t ctrl_meas = 0;
    uint8_t config = 0;

    // Configure CTRL_MEAS register
    ctrl_meas |= (osrs_t << 5); // Temperature oversampling
    ctrl_meas |= (osrs_p << 2); // Pressure oversampling
    ctrl_meas |= mode;          // Mode

    // Configure CONFIG register
    config |= (filter << 2);    // Filter
    config |= (standby << 5);   // Standby time

    // Write to CTRL_MEAS register
    if (bmp280_write_register(bmp, BMP280_REG_CTRL_MEAS, ctrl_meas) != HAL_OK)
        return HAL_ERROR;

    // Write to CONFIG register
    if (bmp280_write_register(bmp, BMP280_REG_CONFIG, config) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

// Read Raw Data Function
HAL_StatusTypeDef bmp280_read_raw(bmp280_t *bmp, int32_t *temperature_raw, int32_t *pressure_raw) {
    // wait until sensor finishes its conversion
    if (bmp280_wait_ready(bmp) != HAL_OK)
        return HAL_ERROR;

    uint8_t data[6];
    if (bmp280_read_registers(bmp, BMP280_REG_PRESS_MSB, data, 6) != HAL_OK)
        return HAL_ERROR;

    *pressure_raw    = ((int32_t)data[0] << 12)
                     | ((int32_t)data[1] <<  4)
                     | ((int32_t)data[2] >>  4);

    *temperature_raw = ((int32_t)data[3] << 12)
                     | ((int32_t)data[4] <<  4)
                     | ((int32_t)data[5] >>  4);

    // Guard: 0x80000 means output disabled / skipped
    if (*temperature_raw == 0x80000 || *pressure_raw == 0x80000)
        return HAL_ERROR;

    return HAL_OK;
}

// Temperature Compensation Function
float bmp280_compensate_temperature(bmp280_t *bmp, int32_t adc_T) {
    float var1, var2;

    var1 = ((float)adc_T / 16384.0f - (float)bmp->dig_T1 / 1024.0f)
         * (float)bmp->dig_T2;

    var2 = ((float)adc_T / 131072.0f - (float)bmp->dig_T1 / 8192.0f)
         * ((float)adc_T / 131072.0f - (float)bmp->dig_T1 / 8192.0f)
         * (float)bmp->dig_T3;

    bmp->t_fine = var1 + var2;          // stored per-instance
    return bmp->t_fine / 5120.0f;
}

// Pressure Compensation Function
float bmp280_compensate_pressure(bmp280_t *bmp, int32_t adc_P) {
    float var1, var2, p;

    var1 = (bmp->t_fine / 2.0f) - 64000.0f;

    var2 = var1 * var1 * (float)bmp->dig_P6 / 32768.0f;
    var2 = var2 + var1 * (float)bmp->dig_P5 * 2.0f;
    var2 = var2 / 4.0f + (float)bmp->dig_P4 * 65536.0f;

    var1 = ((float)bmp->dig_P3 * var1 * var1 / 524288.0f + (float)bmp->dig_P2 * var1) / 524288.0f;
    var1 = (1.0f + var1 / 32768.0f) * (float)bmp->dig_P1;

    if (var1 == 0.0f)
        return 0.0f;    // Avoid division by zero

    p    = 1048576.0f - (float)adc_P;
    p    = ((p - var2 / 4096.0f) * 6250.0f) / var1;

    var1 = (float)bmp->dig_P9 * p * p / 2147483648.0f;
    var2 = p * (float)bmp->dig_P8 / 32768.0f;

    p    = p + (var1 + var2 + (float)bmp->dig_P7) / 16.0f;

    return p / 100.0f;  // Convert to hPa from Pa
}

// Temperature Calculation Function in Fahrenheit
float bmp280_calculate_temperature(float temp_c) {
	  return (temp_c * 9.0f / 5.0f) + 32.0f;
}

// Altitude Calculation Function
float bmp280_calculate_altitude(float pressure, float sea_level_pressure) {
    return 44330.0f * (1.0f - powf(pressure / sea_level_pressure, 0.1903f));
}

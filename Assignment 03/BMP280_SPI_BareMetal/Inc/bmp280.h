/*
 * BMP280.h  —  Bare-metal driver for STM32F446RE
 *
 * Author: Md. Samiul Islam Siam (02)
 *         Partho Kumar Mondal (07)
 */

#ifndef BMP280_H
#define BMP280_H

#include "stm32f446xx.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  I2C address                                                       */
/* ------------------------------------------------------------------ */
#define BMP280_I2C_ADDR         0x76U   /* SDO = GND */

/* ------------------------------------------------------------------ */
/*  Register map                                                      */
/* ------------------------------------------------------------------ */
#define BMP280_REG_CHIP_ID      0xD0U
#define BMP280_REG_RESET        0xE0U
#define BMP280_REG_STATUS       0xF3U
#define BMP280_REG_CTRL_MEAS    0xF4U
#define BMP280_REG_CONFIG       0xF5U
#define BMP280_REG_PRESS_MSB    0xF7U
#define BMP280_REG_CALIB_START  0x88U

/* ------------------------------------------------------------------ */
/*  Known chip IDs                                                    */
/* ------------------------------------------------------------------ */
#define CHIP_ID_BME280          0x60U
#define CHIP_ID_BMP280          0x58U

/* ------------------------------------------------------------------ */
/*  Reset command                                                     */
/* ------------------------------------------------------------------ */
#define BMP280_SOFT_RESET_CMD   0xB6U

/* ------------------------------------------------------------------ */
/*  CS pin helpers — PB9                                              */
/* ------------------------------------------------------------------ */
#define BMP280_CS_LOW()   (GPIOB->BSRR = (1U << (9U + 16U)))
#define BMP280_CS_HIGH()  (GPIOB->BSRR = (1U << 9U))

/* ------------------------------------------------------------------ */
/*  Sea-level pressure reference                                      */
/* ------------------------------------------------------------------ */
#define BMP280_SEA_LEVEL_HPA    1013.25f

/* ------------------------------------------------------------------ */
/*  Communication mode                                                */
/* ------------------------------------------------------------------ */
typedef enum {
    BMP280_MODE_I2C,
    BMP280_MODE_SPI
} bmp280_comm_mode_t;

/* ------------------------------------------------------------------ */
/*  Sensor operating mode                                             */
/* ------------------------------------------------------------------ */
typedef enum {
    BMP280_OPMODE_SLEEP  = 0x00,
    BMP280_OPMODE_FORCED = 0x01,
    BMP280_OPMODE_NORMAL = 0x03
} bmp280_operating_mode_t;

/* ------------------------------------------------------------------ */
/*  Oversampling settings                                             */
/* ------------------------------------------------------------------ */
typedef enum {
    BMP280_OSAMPLE_SKIP = 0x00,
    BMP280_OSAMPLE_1    = 0x01,
    BMP280_OSAMPLE_2    = 0x02,
    BMP280_OSAMPLE_4    = 0x03,
    BMP280_OSAMPLE_8    = 0x04,
    BMP280_OSAMPLE_16   = 0x05
} bmp280_oversampling_t;

/* ------------------------------------------------------------------ */
/*  Standby time (Normal mode)                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    BMP280_STANDBY_0_5_MS   = 0x00,
    BMP280_STANDBY_10_MS    = 0x01,
    BMP280_STANDBY_20_MS    = 0x02,
    BMP280_STANDBY_62_5_MS  = 0x03,
    BMP280_STANDBY_125_MS   = 0x04,
    BMP280_STANDBY_250_MS   = 0x05,
    BMP280_STANDBY_500_MS   = 0x06,
    BMP280_STANDBY_1000_MS  = 0x07
} bmp280_standby_time_t;

/* ------------------------------------------------------------------ */
/*  IIR filter coefficient                                            */
/* ------------------------------------------------------------------ */
typedef enum {
    BMP280_FILTER_OFF = 0x00,
    BMP280_FILTER_2   = 0x01,
    BMP280_FILTER_4   = 0x02,
    BMP280_FILTER_8   = 0x03,
    BMP280_FILTER_16  = 0x04
} bmp280_filter_t;

/* ------------------------------------------------------------------ */
/*  Status codes  (mirrors HAL_StatusTypeDef semantics)               */
/* ------------------------------------------------------------------ */
typedef enum {
    BMP280_OK      = 0x00,
    BMP280_ERROR   = 0x01,
    BMP280_TIMEOUT = 0x03
} bmp280_status_t;

/* ------------------------------------------------------------------ */
/*  Main device structure                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    bmp280_comm_mode_t comm_mode;   /* Communication mode: I2C or SPI */

    /* Calibration data */
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

    float    t_fine;  /* Shared fine temperature for pressure compensation */
} bmp280_t;

/* ------------------------------------------------------------------ */
/*  Compensated output                                                */
/* ------------------------------------------------------------------ */
typedef struct {
    float temperature_c;    /*!< Temperature in °C  */
    float temperature_f;    /*!< Temperature in °F  */
    float pressure_hpa;     /*!< Pressure in hPa    */
    float altitude_m;       /*!< Altitude in meters */
} bmp280_data_t;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief  Initialise the communication peripheral (I2C1 or SPI2)
 *         according to bmp->comm_mode.
 *         Must be called before any other bmp280_* function.
 * @param  bmp  Pointer to bmp280_t device structure.
 */
void bmp280_comm_init(bmp280_t *bmp);

/**
 * @brief  Soft-reset the sensor and verify the chip ID.
 * @param  bmp  Pointer to bmp280_t device structure.
 * @return BMP280_OK on success, BMP280_ERROR otherwise.
 */
bmp280_status_t bmp280_soft_reset(bmp280_t *bmp);

/**
 * @brief  Full initialisation: reset → chip-ID check → calibration read
 *         → apply default configuration (Normal mode, ×1 oversampling,
 *         filter off, 1 s standby).
 * @param  bmp  Pointer to bmp280_t device structure.
 * @return BMP280_OK on success, BMP280_ERROR otherwise.
 */
bmp280_status_t bmp280_init(bmp280_t *bmp);

/**
 * @brief  Write operating-mode, oversampling, filter, and standby
 *         settings to the sensor's control registers.
 * @param  bmp      Pointer to bmp280_t device structure.
 * @param  mode     Operating mode (Sleep / Forced / Normal).
 * @param  osrs_t   Temperature oversampling.
 * @param  osrs_p   Pressure oversampling.
 * @param  filter   IIR filter coefficient.
 * @param  standby  Standby time between Normal-mode measurements.
 * @return BMP280_OK on success, BMP280_ERROR otherwise.
 */
bmp280_status_t bmp280_set_configuration(bmp280_t *bmp,
                                         bmp280_operating_mode_t mode,
                                         bmp280_oversampling_t   osrs_t,
                                         bmp280_oversampling_t   osrs_p,
                                         bmp280_filter_t         filter,
                                         bmp280_standby_time_t   standby);

/**
 * @brief  Read raw 20-bit ADC values from the sensor.
 *         Polls the status register until the conversion is complete.
 * @param  bmp              Pointer to bmp280_t device structure.
 * @param  temperature_raw  Output: raw temperature ADC code.
 * @param  pressure_raw     Output: raw pressure ADC code.
 * @return BMP280_OK on success, BMP280_ERROR / BMP280_TIMEOUT otherwise.
 */
bmp280_status_t bmp280_read_raw(bmp280_t *bmp,
                                int32_t  *temperature_raw,
                                int32_t  *pressure_raw);

/**
 * @brief  Apply Bosch datasheet float compensation to raw temperature.
 *         Also updates bmp->t_fine which bmp280_compensate_pressure() needs.
 * @param  bmp       Pointer to bmp280_t device structure.
 * @param  adc_T     Raw temperature from bmp280_read_raw().
 * @return Temperature in °C.
 */
float bmp280_compensate_temperature(bmp280_t *bmp, int32_t adc_T);

/**
 * @brief  Apply Bosch datasheet float compensation to raw pressure.
 *         Must be called AFTER bmp280_compensate_temperature() so that
 *         bmp->t_fine is up to date.
 * @param  bmp       Pointer to bmp280_t device structure.
 * @param  adc_P     Raw pressure from bmp280_read_raw().
 * @return Pressure in hPa.
 */
float bmp280_compensate_pressure(bmp280_t *bmp, int32_t adc_P);

/**
 * @brief  Convert Celsius to Fahrenheit.
 * @param  temp_c  Temperature in °C.
 * @return Temperature in °F.
 */
float bmp280_calculate_temperature(float temp_c);

/**
 * @brief  Compute barometric altitude from pressure.
 * @param  pressure           Measured pressure in hPa.
 * @param  sea_level_pressure Reference sea-level pressure in hPa.
 * @return Altitude in metres.
 */
float bmp280_calculate_altitude(float pressure, float sea_level_pressure);

/**
 * @brief  Convenience wrapper: read raw ADC values, compensate, and
 *         fill all four fields of *out.
 * @param  bmp  Pointer to bmp280_t device structure.
 * @param  out  Pointer to caller-supplied bmp280_data_t struct.
 * @return BMP280_OK on success, BMP280_ERROR / BMP280_TIMEOUT otherwise.
 */
bmp280_status_t bmp280_read(bmp280_t *bmp, bmp280_data_t *out);

/**
 * @brief  Read the chip-ID register (0xD0).
 * @param  bmp  Pointer to bmp280_t device structure.
 * @return 0x58 for BMP280, 0x60 for BME280, 0x00 on bus error.
 */
uint8_t bmp280_read_chip_id(bmp280_t *bmp);

#endif /* BMP280_H */

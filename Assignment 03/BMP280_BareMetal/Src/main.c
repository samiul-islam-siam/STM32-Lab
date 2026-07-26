/*
 * main.c  —  BMP280 sensor reading via I2C or SPI (bare-metal)
 *
 * Author: Md. Samiul Islam Siam (02)
 *         Partho Kumar Mondal (07)
 */

#include "stm32f446xx.h"
#include "BMP280.h"
#include "helper.h"
#include <stdio.h>
#include <stdint.h>

//  Device instance
static bmp280_t bmp280_inst;
static bmp280_t *BMP280 = &bmp280_inst;

static void Enable_FPU(void)
{
    SCB->CPACR |= (0xFU << 20);
    __DSB();
    __ISB();
}

void I2C_ACK_Test(void) {
	  /* [B2] I2C ACK Test */
	  I2C1->CR1 |= (1 << 8);                  // START
	  while (!(I2C1->SR1 & (1 << 0)));        // SB
	  I2C1->DR = (0x76 << 1);
	  uint32_t timeout = 100000;
	  while (!(I2C1->SR1 & (1 << 1)) && --timeout);

	  if (!timeout)
	      UART_Print("[B2] I2C ACK FAIL\r\n");
	  else {
	      (void)I2C1->SR2;
	      UART_Print("[B2] I2C ACK OK\r\n");
	  }

	  I2C1->CR1 |= (1 << 9); // STOP
}

static void Print_Reading_I2C(const bmp280_data_t *d, uint32_t tick)
{
    char msg[128];

    /* [B3] TIM6 heartbeat */
    sprintf(msg, "[B3] TIM6 Tick: %lu\r\n", (unsigned long)tick);
    UART_Print(msg);

    /* [B4] Plausibility check: 15–40 °C, 900–1100 hPa */
    if (d->temperature_c < 15.0f || d->temperature_c > 40.0f ||
        d->pressure_hpa  < 900.0f || d->pressure_hpa  > 1100.0f)
        UART_Print("[B4] Plausibility FAIL\r\n");
    else
        UART_Print("[B4] Plausibility PASS\r\n");

    sprintf(msg,
        "Temperature : %.2f C / %.2f F\r\n"
        "Pressure    : %.2f hPa\r\n"
        "Altitude    : %.2f m\r\n"
        "----------------------------------------\r\n",
        d->temperature_c,
        d->temperature_f,
        d->pressure_hpa,
        d->altitude_m);
    UART_Print(msg);
}

static void Print_Reading_SPI(const bmp280_data_t *d, uint32_t tick)
{
    char msg[128];

    /* [A3] TIM6 heartbeat */
    sprintf(msg, "[A3] TIM6 Tick: %lu\r\n", (unsigned long)tick);
    UART_Print(msg);

    /* [A4] Plausibility check: 15–40 °C, 900–1100 hPa */
    if (d->temperature_c < 15.0f || d->temperature_c > 40.0f ||
        d->pressure_hpa  < 900.0f || d->pressure_hpa  > 1100.0f)
        UART_Print("[A4] Plausibility FAIL\r\n");
    else
        UART_Print("[A4] Plausibility PASS\r\n");

    sprintf(msg,
        "Temperature : %.2f C / %.2f F\r\n"
        "Pressure    : %.2f hPa\r\n"
        "Altitude    : %.2f m\r\n"
        "----------------------------------------\r\n",
        d->temperature_c,
        d->temperature_f,
        d->pressure_hpa,
        d->altitude_m);
    UART_Print(msg);
}

int main(void)
{
    Enable_FPU();
    SystemClock_Config();
    UART_Config();

    /* Select communication mode here */
    BMP280->comm_mode = BMP280_MODE_I2C;   /* or BMP280_MODE_SPI */
//    BMP280->comm_mode = BMP280_MODE_SPI;   /* or BMP280_MODE_I2C */

    if (BMP280->comm_mode == BMP280_MODE_SPI) {
    	/* [2] UART Loopback test */
    	UART_Print("[A2] UART OK\r\n");
    }

    bmp280_comm_init(BMP280);

    delay_ms(100U);   /* BMP280 power-on settle time */

    UART_Print("\r\n========================================\r\n");
    if (BMP280->comm_mode == BMP280_MODE_SPI) {
    	UART_Print("BMP280 via SPI -- CSE 2206 Lab A\r\n");
    } else {
    	UART_Print("BMP280 via I2C -- CSE 2206 Lab B\r\n");
    }
    UART_Print("========================================\r\n");

    /* [1] Chip ID test */
    char    id_msg[48];
    uint8_t chip_id = bmp280_read_chip_id(BMP280);

    if (chip_id == CHIP_ID_BMP280 && BMP280->comm_mode == BMP280_MODE_SPI)
        sprintf(id_msg, "[A1] ChipID = 0x%02X (expected 0x58)\r\n",  chip_id);
    else if (chip_id == CHIP_ID_BMP280 && BMP280->comm_mode == BMP280_MODE_I2C)
        sprintf(id_msg, "[B1] ChipID = 0x%02X (expected 0x58)\r\n", chip_id);
    else
        sprintf(id_msg, "[ERR] UNKNOWN Sensor! ChipID = 0x%02X\r\n", chip_id);

    UART_Print(id_msg);

    if (chip_id != CHIP_ID_BMP280)
        while (1);   /* Cannot continue without a recognised sensor */


    /* Full init: soft reset → chip ID → calibration → default config */
    if (bmp280_init(BMP280) != BMP280_OK) {
        UART_Print("[ERR] BMP280 Initialization Failed!\r\n");
        while (1);
    }

    delay_ms(100U);   /* Wait for first conversion (~63 ms typical) */

    if (BMP280->comm_mode == BMP280_MODE_I2C) {
        /* [B2] ACK Test */
        I2C_ACK_Test();
    }

    TIM6_Init();

    /* Main loop — 10 readings driven by TIM6 1 Hz tick */
    bmp280_data_t reading;
    uint32_t      tick_count = 0U;

    while (1) {
        if (TIM6_Tick_1Hz && tick_count < 10U) {
            TIM6_Tick_1Hz = 0;   /* Acknowledge the 1 Hz tick */
            tick_count++;

            if (bmp280_read(BMP280, &reading) == BMP280_OK && BMP280->comm_mode == BMP280_MODE_SPI)
                Print_Reading_SPI(&reading, tick_count);
            else if (bmp280_read(BMP280, &reading) == BMP280_OK && BMP280->comm_mode == BMP280_MODE_I2C)
            	Print_Reading_I2C(&reading, tick_count);
            else
                UART_Print("[ERR] BMP280 Read Error!\r\n");
        }

        /* Sleep between ticks to reduce power consumption */
        __WFI();   /* Wakes on every TIM6 IRQ (every 1 ms) */
    }
}

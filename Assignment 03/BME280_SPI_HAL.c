/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ------------------------------------------------------------------ */
/*  BME280 Register Addresses                                          */
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

static int32_t  t_fine;
static float    temp_C, temp_F, pres_hPa, hum_RH;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* CS pin helpers */
#define BME280_CS_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET)
#define BME280_CS_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void BME280_HAL_SPI_WriteReg(uint8_t, uint8_t);
static void BME280_HAL_SPI_ReadRegs(uint8_t, uint8_t *, uint8_t);
static void BME280_HAL_ReadCalibration(void);
static void BME280_HAL_Init(void);
static int32_t BME280_CompensateTemp(int32_t);
static uint32_t BME280_CompensatePressure(int32_t);
static uint32_t BME280_CompensateHumidity(int32_t);
static void BME280_ReadAll_SPI_HAL(void);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static volatile uint32_t ticks_hal = 0;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  // Test A2: UART loopback
  HAL_UART_Transmit(&huart2, (uint8_t *)"[A2] UART OK\r\n", 14, HAL_MAX_DELAY);

  // Banner
  HAL_UART_Transmit(&huart2,
  (uint8_t *)"========================================\r\n"
             "BME280 via SPI -- CSE 2206 Lab A (HAL)\r\n"
             "========================================\r\n", 124, HAL_MAX_DELAY);

  BME280_HAL_Init();

  // Start TIM6 in interrupt mode
  HAL_TIM_Base_Start_IT(&htim6);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 9000-1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 10000-1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* ==================================================================
   A5.2 — HAL SPI Wrapper Functions
   ================================================================== */
static void BME280_HAL_SPI_WriteReg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = { reg & 0x7F, data };
    BME280_CS_LOW();
    HAL_SPI_Transmit(&hspi2, buf, 2, HAL_MAX_DELAY);
    BME280_CS_HIGH();
}

static void BME280_HAL_SPI_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t cmd = reg | 0x80;
    BME280_CS_LOW();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive (&hspi2, buf, len, HAL_MAX_DELAY);
    BME280_CS_HIGH();
}

/* ==================================================================
   Calibration Read
   ================================================================== */
static void BME280_HAL_ReadCalibration(void)
{
    uint8_t buf[26];

    BME280_HAL_SPI_ReadRegs(0x88, buf, 24);
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

    BME280_HAL_SPI_ReadRegs(0xA1, buf, 1);
    dig_H1 = buf[0];

    BME280_HAL_SPI_ReadRegs(0xE1, buf, 7);
    dig_H2 = (int16_t)(buf[1] << 8) | buf[0];
    dig_H3 = buf[2];
    dig_H4 = (int16_t)(buf[3] << 4) | (buf[4] & 0x0F);
    dig_H5 = (int16_t)(buf[5] << 4) | (buf[4] >> 4);
    dig_H6 = (int8_t) buf[6];
}

/* ==================================================================
   BME280 Init
   ================================================================== */
static void BME280_HAL_Init(void)
{
    uint8_t id;

    /* Soft reset */
    BME280_HAL_SPI_WriteReg(BME280_REG_RESET, 0xB6);
    HAL_Delay(10);

    /* Chip ID */
    BME280_HAL_SPI_ReadRegs(BME280_REG_CHIP_ID, &id, 1);
    {
        char s[64];
        sprintf(s, "[A1] ChipID=0x%02X (expect 0x60)\r\n", id);
        HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
    }

    BME280_HAL_ReadCalibration();

    BME280_HAL_SPI_WriteReg(BME280_REG_CTRL_HUM,  0x01); /* osrs_h=x1        */
    BME280_HAL_SPI_WriteReg(BME280_REG_CTRL_MEAS, 0x57); /* osrs_t/p, normal */
    BME280_HAL_SPI_WriteReg(BME280_REG_CONFIG,    0x10); /* filter=16, 0.5ms */

    HAL_Delay(100);
}

/* ==================================================================
   Compensation Formulas
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
   Read All Sensor Data
   ================================================================== */
static void BME280_ReadAll_SPI_HAL(void)
{
    uint8_t raw[8];
    BME280_HAL_SPI_ReadRegs(BME280_REG_PRESS_MSB, raw, 8);

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
   A5.3 — HAL TIM6 Period-Elapsed Callback
   ================================================================== */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        ticks_hal++;

        /* Test A3 heartbeat */
        {
            char s[32];
            sprintf(s, "[A3] Tick:%lu\r\n", ticks_hal);
            HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
        }

        BME280_ReadAll_SPI_HAL();

        /* Test A4 plausibility */
        if (temp_C < 15.0f || temp_C > 40.0f)
            HAL_UART_Transmit(&huart2, (uint8_t *)"[A4] Temp FAIL\r\n", 16, HAL_MAX_DELAY);
        else if (pres_hPa < 900 || pres_hPa > 1100)
            HAL_UART_Transmit(&huart2, (uint8_t *)"[A4] Pres FAIL\r\n", 16, HAL_MAX_DELAY);
        else if (hum_RH < 0 || hum_RH > 100)
            HAL_UART_Transmit(&huart2, (uint8_t *)"[A4] Hum  FAIL\r\n", 16, HAL_MAX_DELAY);
        else
            HAL_UART_Transmit(&huart2, (uint8_t *)"[A4] Plausibility PASS\r\n", 24, HAL_MAX_DELAY);

        /* Main sensor output */
        char msg[128];
        sprintf(msg,
            "[SPI-HAL] Temp:%.2fC Pres:%.2fhPa Hum:%.2f%%\r\n",
            temp_C, pres_hPa, hum_RH);
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

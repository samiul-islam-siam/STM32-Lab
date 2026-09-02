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
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    uint32_t marker;          /* IDENTITY_MARKER if valid */
    char     registration[16];
    char     roll[12];
    char     name[32];
} StudentInfo_t;              /* 64 bytes, word-aligned */

typedef struct {
    uint32_t      marker;      /* IDENTITY_MARKER if this block was provisioned */
    StudentInfo_t student[2];
} IdentityBlock_t;

typedef struct {
    uint32_t marker;          /* RESULTS_MARKER if valid */
    uint32_t mv_12bit;        /* stored as millivolts (fits in one word) */
    uint32_t mv_10bit;
    uint32_t mv_8bit;
    uint32_t mv_6bit;
} TestResults_t;              /* 20 bytes -> 5 words */

typedef enum {
	RES_12BIT = 0,
	RES_10BIT,
	RES_8BIT,
	RES_6BIT,
	RES_COUNT
} Resolution_t;

typedef struct {
    const char *label;
    uint32_t    halres;   /* HAL ADC resolution constant */
    uint32_t    maxcode;
} ResInfo_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VREF_MV              3300U   /* mV, Nucleo VDDA ~ 3.3V          */
#define N_SAMPLES            16      /* averaged samples per resolution */

#define FLASH_SECTOR6_BASE   0x08040000U   /* identity block (write-once) */
#define FLASH_SECTOR7_BASE   0x08060000U   /* test-results block          */
#define FLASH_SECTOR6_NUM    6U
#define FLASH_SECTOR7_NUM    7U

#define IDENTITY_MARKER      0xB1010001U
#define RESULTS_MARKER       0xCAFEBABEU
#define ERASED_WORD          0xFFFFFFFFU

#define UART_DEBOUNCE_MS     50U      /* swallow bursts from one keypress */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static const ResInfo_t kResTable[RES_COUNT] = {
    { "12-bit", ADC_RESOLUTION_12B, 4095 },
    { "10-bit", ADC_RESOLUTION_10B, 1023 },
    { "8-bit",  ADC_RESOLUTION_8B,   255 },
    { "6-bit",  ADC_RESOLUTION_6B,    63 },
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void FPU_Enable(void);
static void UART_SendStr(const char *s);

static void Flash_EraseSector(uint8_t sectorNum);
static void Flash_WriteWord(uint32_t addr, uint32_t data);
static void Flash_WriteBlock(uint32_t addr, const void *src, uint32_t len);

static void Identity_FillSlot(StudentInfo_t *slot, const char *reg,
                               const char *roll, const char *name);
static void Identity_ProvisionPair(const char *reg0, const char *roll0, const char *name0,
                                   const char *reg1, const char *roll1, const char *name1);
static void Identity_Display(void);

static void ADC_SetResolution(Resolution_t r);
static uint16_t ADC_ReadRaw(void);
static float CodeToVolts(uint32_t avgCode, uint32_t maxCode);
static void RunTestSuite(void);
static void Results_Display(void);

static void WaitAndDebounceTrigger(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void FPU_Enable(void)
{
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    __DSB();
    __ISB();
}

static void UART_SendStr(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

/* =====================================================================
 * Milestone B - Flash utility functions
 * ===================================================================== */

static void Flash_EraseSector(uint8_t sectorNum)
{
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t sectorError;

    HAL_FLASH_Unlock();

    eraseInit.TypeErase    = FLASH_TYPEERASE_SECTORS;
    eraseInit.Sector       = sectorNum;
    eraseInit.NbSectors    = 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASHEx_Erase(&eraseInit, &sectorError);

    HAL_FLASH_Lock();
}

static void Flash_WriteWord(uint32_t addr, uint32_t data)
{
    HAL_FLASH_Unlock();
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data);
    HAL_FLASH_Lock();
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

    UART_SendStr("\r\n========== Student Identity ==========\r\n");
    if (block->marker == IDENTITY_MARKER) {
        for (int i = 0; i < 2; i++) {
            const StudentInfo_t *info = &block->student[i];
            if (info->marker != IDENTITY_MARKER) {
                continue;
            }
            snprintf(line, sizeof(line), "----- Member %d -----\r\n", i + 1);
            UART_SendStr(line);
            snprintf(line, sizeof(line), "Registration: %s\r\n", info->registration);
            UART_SendStr(line);
            snprintf(line, sizeof(line), "Roll:         %s\r\n", info->roll);
            UART_SendStr(line);
            snprintf(line, sizeof(line), "Name:         %s\r\n", info->name);
            UART_SendStr(line);
        }
    } else {
        UART_SendStr("Not yet provisioned.\r\n");
    }
}

/* =====================================================================
 * Milestone A - ADC acquisition
 * ===================================================================== */

static void ADC_SetResolution(Resolution_t r)
{
    hadc1.Init.Resolution = kResTable[r].halres;
    HAL_ADC_Init(&hadc1);
}

static uint16_t ADC_ReadRaw(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    uint16_t raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return raw;
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

    UART_SendStr("\r\nRunning multi-resolution test suite...\r\n");

    for (Resolution_t r = RES_12BIT; r < RES_COUNT; r++) {
        ADC_SetResolution(r);

        /* allow resolution change to settle before sampling */
        HAL_Delay(1);

        uint32_t sum = 0;
        for (int s = 0; s < N_SAMPLES; s++) {
            sum += ADC_ReadRaw();
        }
        uint32_t avgCode = sum / N_SAMPLES;
        float volts = CodeToVolts(avgCode, kResTable[r].maxcode);

        char line[64];
        snprintf(line, sizeof(line), "%s: avg_code=%lu  V=%.3f\r\n",
                           kResTable[r].label, (unsigned long)avgCode, volts);
        UART_SendStr(line);

        /* Flash storage stays as integer millivolts -- convert back at the boundary. */
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

    UART_SendStr("Results stored to Sector 7.\r\n");
}

/* =====================================================================
 * Milestone E - Boot sequence & result display
 * ===================================================================== */

static void Results_Display(void)
{
    char line[64];
    const TestResults_t *res = (const TestResults_t *)FLASH_SECTOR7_BASE;

    UART_SendStr("====== Previous Testing Results ======\r\n");
    if (res->marker == RESULTS_MARKER) {
        uint32_t vals[4] = { res->mv_12bit, res->mv_10bit, res->mv_8bit, res->mv_6bit };
        for (int i = 0; i < RES_COUNT; i++) {
            snprintf(line, sizeof(line), "%s: %.3f V\r\n",
                                kResTable[i].label, (float)vals[i] / 1000.0f);
            UART_SendStr(line);
        }
    } else {
        UART_SendStr("No previous test data.\r\n");
    }
}

/* Debounce a UART trigger burst: once a byte is seen, drain and ignore
 * any further bytes that arrive within UART_DEBOUNCE_MS. */
static void WaitAndDebounceTrigger(void)
{
    uint8_t b;

    HAL_UART_Receive(&huart2, &b, 1, HAL_MAX_DELAY);   /* wait for first byte */

    HAL_Delay(UART_DEBOUNCE_MS);
    while (HAL_UART_Receive(&huart2, &b, 1, 1) == HAL_OK) {
        HAL_Delay(UART_DEBOUNCE_MS);    /* burst still going -- keep draining */
    }
}

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
  FPU_Enable();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  UART_SendStr("\r\n\r\n----- CSE2206 Lab04: ADC & Flash -----\r\n");

  /* ---- One time provisioning path -------------------------------
   * Uncomment ONCE to provision identities and reflash, then comment it back out.
   * This is intentional to NOT run it automatically after every boot.
   * ----------------------------------------------------------------*/

  //    Identity_ProvisionPair("2023-915-945", "2", "Md. Samiul Islam Siam",
  //                           "2023-315-950", "7", "Partho Kumar Mondal");

  /* Boot sequence (Milestone E) */
  Identity_Display();
  Results_Display();

  UART_SendStr("\r\nSend any byte over UART to run the test suite...\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    WaitAndDebounceTrigger();
    RunTestSuite();
    UART_SendStr("\r\nSend any byte to run again...\r\n");
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
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

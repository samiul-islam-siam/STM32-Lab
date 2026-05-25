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
#define N  100U



/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
// PROFILE macro for printing output
#define PROFILE(num, label, block)                                          \
    do {                                                                    \
        uint32_t _t0_dwt  = DWT_GetCycles();                                \
        uint32_t _t0_tim2 = TIM2_GetMicros();                               \
        { block }                                                           \
        uint32_t _t1_dwt  = DWT_GetCycles();                                \
        uint32_t _t1_tim2 = TIM2_GetMicros();                               \
        uint32_t _cyc     = _t1_dwt  - _t0_dwt;                             \
        uint32_t _us_tim2 = _t1_tim2 - _t0_tim2;                            \
        Profile_PrintRow(num, label, "DWT ", _cyc,  0U);                    \
        Profile_PrintRow(num, label, "TIM2", 0U,    _us_tim2);              \
        UART2_Print(                                                        \
        "    |                          |       |             |             |           |      \r\n"); \
    } while (0)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static int arr[N]; // for bubble sort

static volatile uint32_t isqrt_result; // for square root

static uint8_t src_buf[512U]; // for memory copy
static uint8_t dst_buf[512U];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void UART2_Print(const char *);
static void delay_us(uint16_t);
static void delay_ms(uint32_t);

static void DWT_Init(void);
static inline uint32_t DWT_GetCycles(void);
static inline uint32_t TIM2_GetMicros(void);

static void BubbleSort_PrepareWorstCase(void);
static void BubbleSort(void);

static uint32_t isqrt(uint32_t);
static void IsqrtBenchmark(void);

static void MemCopy_ByteByByte(void);

static void Profile_PrintRow(const char *, const char *, const char *, uint32_t, uint32_t);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* Start free-running timers */
  HAL_TIM_Base_Start(&htim6);   /* delay helper      */
  HAL_TIM_Base_Start(&htim2);   /* profiling stopwatch */

  /* Enable DWT cycle counter (no CubeMX tab — done in code) */
  DWT_Init();

  /* Initialise source buffer for memcpy benchmark */
  for (uint32_t i = 0; i < 512U; i++)
      src_buf[i] = (uint8_t)(i & 0xFFU);

  /* ── Table header ───────────────────────────────────────────── */
  UART2_Print("\r\n");
  UART2_Print("===== Task 3: Duration Measurement & Code Profiling (HAL) =====\r\n\r\n");
  UART2_Print(
      "#   |Block Description         |Method |Cycles       |ns           |us         |ms    \r\n"
      "----|--------------------------|-------|-------------|-------------|-----------|------\r\n");

  /* ── [1] Bubble sort worst case ─────────────────────────────── */
  BubbleSort_PrepareWorstCase();
  PROFILE("[1]", "Bubble sort N=100 (worst)",
      BubbleSort();
  );

  /* ── [2] delay_ms(100)  — expected ~100 000 µs ──────────────── */
  PROFILE("[2]", "delay_ms(100)",
      delay_ms(100U);
  );

  /* ── [3] UART SendString 48 chars
     At 115200 8N1: 48 bytes × 10 bits / 115200 ≈ 4167 µs expected ── */
  PROFILE("[3]", "SendString 48B",
      UART2_Print("PROFILING: STM32F446RE USART2 @ 115200 baud OK!\r\n");
  );

  /* ── [4] Integer sqrt × 1000 inputs ─────────────────────────── */
  PROFILE("[4]", "isqrt() x1000 inputs",
      IsqrtBenchmark();
  );

  /* ── [5] Byte-by-byte memcpy 512 B ──────────────────────────── */
  PROFILE("[5]", "MemCopy byte x512",
      MemCopy_ByteByByte();
  );

  UART2_Print(
      "----|--------------------------|-------|-------------|-------------|-----------|------\r\n");
  UART2_Print("===== Task 3 Complete =====\r\n\r\n");

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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 89;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  htim6.Init.Prescaler = 89;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* UART helper */
static void UART2_Print(const char *str)
{
    HAL_UART_Transmit(&huart2,
                      (const uint8_t *)str,
                      (uint16_t)strlen(str),
                      HAL_MAX_DELAY);
}

static void delay_us(uint16_t us)
{
    uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim6);
    while ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim6) - start) < us) {}
}

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) delay_us(1000U);
}

/* ================================================================
 * Method A — DWT Cycle Counter
 * ARM Cortex-M4 hardware counter: 1 tick = 1 CPU cycle = 5.56 ns @ 180 MHz
 * Max range: 2^32 cycles ≈ 23.9 s before overflow
 * ================================================================ */
static void DWT_Init(void)
{
    /* Enable trace subsystem */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset and enable cycle counter */
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t DWT_GetCycles(void)
{
    return DWT->CYCCNT;
}

/* ================================================================
 * Method B — TIM2 Free-Running µs Stopwatch
 * 32-bit counter, 1 µs resolution, overflows at ~71 min
 * ================================================================ */
static inline uint32_t TIM2_GetMicros(void)
{
    return (uint32_t)__HAL_TIM_GET_COUNTER(&htim2);
}

/* ================================================================
 * Code Blocks Under Test
 * ================================================================ */

/* ---- Block 1: Bubble Sort (worst case: reverse-sorted) ---- */
static void BubbleSort_PrepareWorstCase(void)
{
    for (uint32_t i = 0; i < N; i++)
        arr[i] = (int)(N - i);   /* 100, 99, …, 2, 1 */
}

static void BubbleSort(void)
{
    int temp;
    for (uint32_t i = 0; i < N - 1U; i++)
    {
        for (uint32_t j = 0; j < N - 1U - i; j++)
        {
            if (arr[j] > arr[j + 1U])
            {
                temp        = arr[j];
                arr[j]      = arr[j + 1U];
                arr[j + 1U] = temp;
            }
        }
    }
}

/* ---- Block 3: Integer Square Root (Newton-Raphson) x 1000 ---- */
static uint32_t isqrt(uint32_t n)
{
    if (n == 0U) return 0U;
    uint32_t x = n;
    uint32_t y = (x + 1U) / 2U;
    while (y < x)
    {
        x = y;
        y = (x + n / x) / 2U;
    }
    return x;
}

static void IsqrtBenchmark(void)
{
    for (uint32_t i = 0; i < 1000U; i++)
        isqrt_result = isqrt(i * 7U + 1U);
}

/* ---- Block 4: Byte-by-byte memory copy, 512 bytes ---- */
static void MemCopy_ByteByByte(void)
{
    for (uint32_t i = 0; i < 512U; i++)
        dst_buf[i] = src_buf[i];
}

/* ================================================================
 * Output Formatting
 *
 * When cycles > 0  → DWT row  (timing derived from cycle count)
 * When cycles == 0 → TIM2 row (timing comes from us_val directly)
 * ================================================================ */
static void Profile_PrintRow(const char *num, const char *label, const char *method,
                             uint32_t cycles, uint32_t us_val)
{
    char buf[160];

    if (cycles > 0U)   /* DWT path */
    {
        /* ns = cycles × 1000 / 180  — uses 64-bit to prevent overflow */
        uint32_t ns_precise = (uint32_t)((uint64_t)cycles * 1000U / 180U);

        snprintf(buf, sizeof(buf),
            "%-4s|%-26s|%-7s|%-13lu|%-13lu|%-11lu|%-6lu\r\n",
            num, label, method,
            (unsigned long)cycles,
            (unsigned long)ns_precise,
            (unsigned long)(ns_precise / 1000U),
            (unsigned long)(ns_precise / 1000000U));
    }
    else               /* TIM2 path */
    {
        uint32_t ns_val = us_val * 1000U;
        uint32_t ms_val = us_val / 1000U;

        snprintf(buf, sizeof(buf),
            "%-4s|%-26s|%-7s|%-15s|%-13lu|%-11lu|%-6lu\r\n",
            num, label, method,
            "—",
            (unsigned long)ns_val,
            (unsigned long)us_val,
            (unsigned long)ms_val);
    }

    UART2_Print(buf);
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

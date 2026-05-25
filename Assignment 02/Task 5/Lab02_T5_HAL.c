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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct { uint8_t r, g, b; } LED_t;
typedef struct { const char *name; uint8_t r, g, b; } Colour_t;

static const Colour_t palette[] =
{
    {"Red",            255,   0,   0},
    {"Green",            0, 255,   0},
    {"Blue",             0,   0, 255},
    {"Yellow",         255, 255,   0},
    {"Cyan",             0, 255, 255},
    {"Magenta",        255,   0, 255},
    {"White",          255, 255, 255},
    {"Warm White",     255, 200,  80},
    {"Cool White",     215, 235, 255},
    {"DU Blue",         31,  56, 100},
    {"Off",              0,   0,   0}
};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WS_ARR        225U   /* TIM1 ARR: 226 ticks = 1.256 µs period        */
#define WS_T1H        144U   /* Logic-1 high time: ~0.8 µs                   */
#define WS_T0H         72U   /* Logic-0 high time: ~0.4 µs                   */
#define WS_RESET       50U   /* Reset: 50 × 0-duty entries ≥ 50 µs LOW       */
#define NUM_LEDS        5U   /* Physical LED chain length                     */

#define PWM_BUF_SIZE  ((NUM_LEDS * 24U) + WS_RESET)
#define PALETTE_COUNT  (sizeof(palette) / sizeof(palette[0]))
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim6;
DMA_HandleTypeDef hdma_tim1_ch1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint16_t pwmData[PWM_BUF_SIZE];

static LED_t g_leds[NUM_LEDS];

/* Flag set by HAL PWM DMA-complete callback */
static volatile uint8_t ws_transfer_done = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */
static void UART_Print(const char *);
static void delay_us(uint16_t);
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);
static void WS2812B_Send(void);
static void WS2812_SetAll(uint8_t, uint8_t, uint8_t);
static void WS2812_SetHue(uint16_t);

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
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  char buf[128];

  UART_Print("\r\n===== Task 5: WS2812B Colour Mixing and Animation (HAL) =====\r\n");

  /* Power-on clear */
  WS2812_SetAll(0, 0, 0);
  HAL_Delay(100U);

  UART_Print("\r\n[1] Colour palette — 1 s per colour\r\n");

  for (uint32_t i = 0; i < PALETTE_COUNT; i++)
  {
      WS2812_SetAll(palette[i].r, palette[i].g, palette[i].b);

      snprintf(buf, sizeof(buf),
          "Colour: %-12s R=%3u G=%3u B=%3u  GRB=[%02X %02X %02X]\r\n",
          palette[i].name,
          palette[i].r, palette[i].g, palette[i].b,
          palette[i].g, palette[i].r, palette[i].b);
      UART_Print(buf);

      HAL_Delay(1000U);
  }

  UART_Print("\r\n[2] Hue sweep 0-359 (step 3, 25 ms/step)\r\n");

  for (uint16_t h = 0; h < 360U; h += 3U)
  {
      WS2812_SetHue(h);
      HAL_Delay(25U);
  }
  WS2812_SetAll(0, 0, 0);
  UART_Print("Hue sweep complete.\r\n");

  UART_Print("\r\n[3] Colour chase — 5 LEDs, 3 rounds\r\n");

  for (int round = 0; round < 3; round++)
  {
      for (uint32_t active = 0; active < NUM_LEDS; active++)
      {
          for (uint32_t j = 0; j < NUM_LEDS; j++)
              g_leds[j] = (LED_t){0, 0, 0};

          g_leds[active] = (LED_t){255, 0, 0};
          WS2812B_Send();

          snprintf(buf, sizeof(buf),
              "  Round %d — Active LED: %lu/%lu\r\n",
              round + 1,
              (unsigned long)(active + 1U),
              (unsigned long)NUM_LEDS);
          UART_Print(buf);

          HAL_Delay(200U);
      }
  }

  WS2812_SetAll(0, 0, 0);
  UART_Print("\r\n===== Task 5 Complete =====\r\n");

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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 225;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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
  htim6.Init.Prescaler = 179;
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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

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
/* ═══════════════════════════════════════════════════════════════════════════
 * USART helper
 * ═══════════════════════════════════════════════════════════════════════════ */
static void UART_Print(const char *s)
{
    HAL_UART_Transmit(&huart2, (const uint8_t *)s, (uint16_t)strlen(s), 1000U);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Microsecond delay (TIM6 — prescaler must give 1 µs tick)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    HAL_TIM_Base_Start(&htim6);
    while (__HAL_TIM_GET_COUNTER(&htim6) < us) {}
    HAL_TIM_Base_Stop(&htim6);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HAL callback — fires when DMA finishes the PWM burst
 *
 * Override the weak definition provided by stm32f4xx_hal_tim.c.
 * CubeMX registers the DMA stream with htim1, so HAL calls this
 * automatically via the DMA TC interrupt.
 * ═══════════════════════════════════════════════════════════════════════════ */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1); /* Stop TIM + DMA      */
        ws_transfer_done = 1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WS2812B_Send  — build the bit-stream then fire it via DMA-PWM
 * ═══════════════════════════════════════════════════════════════════════════ */
static void WS2812B_Send(void)
{
    uint32_t idx = 0;

    /* ── Step 1: Build GRB bit-stream, MSB first ── */
    for (uint32_t led = 0; led < NUM_LEDS; led++)
    {
        uint32_t color = ((uint32_t)g_leds[led].g << 16)
                       | ((uint32_t)g_leds[led].r <<  8)
                       |  (uint32_t)g_leds[led].b;

        for (int bit = 23; bit >= 0; bit--)
            pwmData[idx++] = (color & (1U << bit)) ? WS_T1H : WS_T0H;
    }

    /* ── Step 2: Append reset guard (≥ 50 µs of LOW output) ── */
    for (uint32_t i = 0; i < WS_RESET; i++)
        pwmData[idx++] = 0U;

    /* ── Step 3: Arm the transfer-done flag ── */
    ws_transfer_done = 0;

    /*
     * ── Step 4: Start PWM + DMA burst ──
     *
     * HAL_TIM_PWM_Start_DMA():
     *   • Writes M0AR, NDTR, enables DMA stream
     *   • Enables TIM1_CH1 DMA request (CC1DE)
     *   • Enables TIM1 (CEN) — first CC1 match triggers first DMA beat
     *   • Registers internal DMA callbacks so HAL_TIM_PWM_PulseFinishedCallback
     *     fires when NDTR reaches 0
     *
     * Cast pwmData to uint32_t* is required by the HAL prototype even though
     * the actual transfer width is 16-bit (configured in CubeMX DMA settings).
     */
    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1,
                           (uint32_t *)pwmData, idx);

    /* ── Step 5: Poll until DMA TC callback clears the flag ── */
    while (!ws_transfer_done) {}

    /*
     * HAL_TIM_PWM_Stop_DMA() is already called inside the callback;
     * nothing more needed here.
     */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * High-level LED helpers
 * ═══════════════════════════════════════════════════════════════════════════ */
static void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint32_t i = 0; i < NUM_LEDS; i++)
        g_leds[i] = (LED_t){r, g, b};
    WS2812B_Send();
}

static void WS2812_SetHue(uint16_t H)
{
    H = H % 360U;
    uint8_t seg  = (uint8_t)(H / 60U);
    uint8_t frac = (uint8_t)(H % 60U);
    uint8_t q    = (uint8_t)(255U * (60U - frac) / 60U);
    uint8_t t    = (uint8_t)(255U * frac          / 60U);
    uint8_t r, g, b;

    switch (seg)
    {
        case 0: r=255; g=t;   b=0;   break;
        case 1: r=q;   g=255; b=0;   break;
        case 2: r=0;   g=255; b=t;   break;
        case 3: r=0;   g=q;   b=255; break;
        case 4: r=t;   g=0;   b=255; break;
        case 5: r=255; g=0;   b=q;   break;
        default: r=0;  g=0;   b=0;   break;
    }
    WS2812_SetAll(r, g, b);
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

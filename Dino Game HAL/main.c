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
#include "lcd.h"
#include <stdlib.h>  // for random()
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// LCD pin arrays: order must match D4, D5, D6, D7
Lcd_PortType dataPorts[] = { GPIOC, GPIOB, GPIOA, GPIOA };
Lcd_PinType  dataPins[]  = { GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_6 };

Lcd_HandleTypeDef lcd;

// CUSTOM CHARACTER BITMAPS
uint8_t DINO_RIGHT_FOOT_PART_1[8] = {0x00,0x00,0x02,0x02,0x03,0x03,0x01,0x01};
uint8_t DINO_RIGHT_FOOT_PART_2[8] = {0x07,0x07,0x07,0x04,0x1C,0x1C,0x18,0x00};
uint8_t DINO_LEFT_FOOT_PART_1[8]  = {0x00,0x00,0x02,0x02,0x03,0x03,0x01,0x00};
uint8_t DINO_LEFT_FOOT_PART_2[8]  = {0x07,0x07,0x07,0x04,0x1C,0x1C,0x18,0x08};
uint8_t CACTUS_PART_1[8]          = {0x00,0x04,0x04,0x14,0x14,0x1C,0x04,0x04};
uint8_t CACTUS_PART_2[8]          = {0x04,0x05,0x05,0x15,0x1F,0x04,0x04,0x04};
uint8_t BIRD_WINGS_PART_1[8]      = {0x01,0x01,0x01,0x01,0x09,0x1F,0x00,0x00};
uint8_t BIRD_WINGS_PART_2[8]      = {0x00,0x10,0x18,0x1C,0x1E,0x1F,0x00,0x00};

// GAME STATE VARIABLES
// --- Dino position ---
int dino_col1 = 1;       // LCD column of dino's left half
int dino_col2 = 2;       // LCD column of dino's right half
int dino_row  = 1;       // LCD row of dino (0 = top, 1 = bottom)

// --- Leg animation ---
uint32_t timer1 = 0;     // timestamp of last animation update
int period1     = 100;   // animation update interval (ms)
int flag        = 1;     // toggles between 1 and 2 to alternate leg frames

// --- Obstacle position ---
int obstacle_row = 0;    // row of current obstacle (0 = bird, 1 = cactus)
int obstacle_col = 13;   // current column of obstacle, moves left each tick

// --- Obstacle movement ---
uint32_t timer2 = 0;     // timestamp of last obstacle move
int period2     = 100;   // obstacle move interval (ms), decreases over time to speed up

// --- Internal state flags ---
int a = 0;               // 1 = obstacle needs to be redrawn this cycle
int b = 1;               // left collision column for cactus (set to 50 during jump to disable)
int c = 2;               // right collision column for cactus (set to 50 during jump to disable)
int d = 0;               // 0 = dino on ground, 1 = dino jumping

// --- Score timing ---
uint32_t timer3 = 0;     // timestamp of last score increment
int period3     = 100;   // score increment interval (ms)

// --- Score values ---
int score          = 0;  // current score, resets to 0 every 100 points
int score_hundreds = 0;  // how many times score has rolled over 100

// --- Obstacle type ---
int random_num = 0;      // 0 = bird, 1 = cactus type A, 2 = cactus type B

// --- Bird position ---
int bird_col = 13;       // column of bird's left half (right half is bird_col + 1)

// --- Speed control ---
int f            = 13;   // column just ahead of obstacle, used to erase previous position
int acceleration = 1;    // how many ms to subtract from period2 each obstacle cycle

// --- Jump sound timing ---
uint32_t timer4 = 0;     // timestamp of last jump sound
int period4     = 800;   // minimum interval between jump sounds (ms)

// --- Button state tracking ---
int btn_curr = 0;        // current button reading this cycle
int btn_prev = 0;        // button reading from previous cycle, used to detect state change

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void buzz_tone(uint32_t frequency, uint32_t duration_ms) {
    if (frequency == 0) {
        HAL_Delay(duration_ms);
        return;
    }

    uint32_t half_period_us = 500000 / frequency; // in microseconds
    uint32_t cycles = (duration_ms * 1000) / (2 * half_period_us);

    for (uint32_t i = 0; i < cycles; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, 1);
        // busy-wait microsecond delay (HSI 16MHz ≈ 16 cycles/us)
        for (volatile uint32_t d = 0; d < (half_period_us * 16); d++);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, 0);
        for (volatile uint32_t d = 0; d < (half_period_us * 16); d++);
    }
}

void gameOver(void) {
//  buzz_tone(200, 250);
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 4);
    Lcd_string(&lcd, "GAME OVER!");
    Lcd_cursor(&lcd, 1, 3);
    Lcd_string(&lcd, "Score:");
    Lcd_cursor(&lcd, 1, 9);
    Lcd_int(&lcd, score_hundreds);
    Lcd_write_char(&lcd, (uint8_t)score_hundreds + '0');
    Lcd_int(&lcd, score);
    while(1);  // halt forever, press reset button to play again
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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  lcd = Lcd_create(dataPorts, dataPins,
                   GPIOB, GPIO_PIN_5,   // RS
                   GPIOB, GPIO_PIN_4,   // EN
                   LCD_4_BIT_MODE);

  // Define all custom characters (slots 0–7)
  Lcd_define_char(&lcd, 0, DINO_RIGHT_FOOT_PART_1);
  Lcd_define_char(&lcd, 1, DINO_RIGHT_FOOT_PART_2);
  Lcd_define_char(&lcd, 2, DINO_RIGHT_FOOT_PART_1);
  Lcd_define_char(&lcd, 3, DINO_RIGHT_FOOT_PART_2);
  Lcd_define_char(&lcd, 4, DINO_LEFT_FOOT_PART_1);
  Lcd_define_char(&lcd, 5, DINO_LEFT_FOOT_PART_2);
  Lcd_define_char(&lcd, 6, CACTUS_PART_1);
  Lcd_define_char(&lcd, 7, CACTUS_PART_2);

  srand(HAL_GetTick()); // seed random number generator

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */

    uint32_t now = HAL_GetTick();

    // --- Dino leg animation timer ---
    if (now > timer1 + period1) {
        timer1 = now;
        flag = (flag == 1) ? 2 : 1;
    }

    // --- Obstacle movement timer ---
    if (now > timer2 + period2) {
        timer2 = now;
        obstacle_col--;
        if (obstacle_col < 0) {
            obstacle_col = 13;
            if (period2 > 30) period2 -= acceleration;
            random_num = rand() % 3;
        }
        f = obstacle_col + 1;
        Lcd_cursor(&lcd, 1, f); Lcd_string(&lcd, " ");
        Lcd_cursor(&lcd, 0, f); Lcd_string(&lcd, " ");
        Lcd_cursor(&lcd, 1, 0); Lcd_string(&lcd, " ");
        Lcd_cursor(&lcd, 0, 0); Lcd_string(&lcd, " ");
        a = 1;
    }

    // --- Draw dino on ground ---
    if (d == 0) {
        if (flag == 1) {
            Lcd_cursor(&lcd, dino_row, dino_col1); Lcd_write_char(&lcd, 2);
            Lcd_cursor(&lcd, dino_row, dino_col2); Lcd_write_char(&lcd, 3);
        } else {
            Lcd_cursor(&lcd, dino_row, dino_col1); Lcd_write_char(&lcd, 4);
            Lcd_cursor(&lcd, dino_row, dino_col2); Lcd_write_char(&lcd, 5);
        }
    }

    // --- Draw obstacle ---
    if (a == 1) {
        if (random_num == 1) {
            obstacle_row = 1;
            Lcd_define_char(&lcd, 6, CACTUS_PART_1);
            Lcd_cursor(&lcd, obstacle_row, obstacle_col);
            Lcd_write_char(&lcd, 6);
        } else if (random_num == 2) {
            obstacle_row = 1;
            Lcd_define_char(&lcd, 7, CACTUS_PART_2);
            Lcd_cursor(&lcd, obstacle_row, obstacle_col);
            Lcd_write_char(&lcd, 7);
        } else {
            bird_col = obstacle_col - 1;
            obstacle_row = 0;
            Lcd_define_char(&lcd, 6, BIRD_WINGS_PART_1);
            Lcd_cursor(&lcd, obstacle_row, bird_col);
            Lcd_write_char(&lcd, 6);
            Lcd_define_char(&lcd, 7, BIRD_WINGS_PART_2);
            Lcd_cursor(&lcd, obstacle_row, obstacle_col);
            Lcd_write_char(&lcd, 7);
        }
        a = 0;
    }

    // --- Collisions ---
    // Bird collision: dino is jumping, bird reaches dino columns
    if (obstacle_row == 0 && d == 1 &&
        (obstacle_col == 1 || bird_col == 1)) {
//    	buzz_tone(200, 250);
        gameOver();
    }

    // Cactus collision: dino on ground, cactus reaches dino columns
    if (obstacle_row == 1 && d == 0 && obstacle_col == 1) {
//      buzz_tone(200, 250);
        gameOver();
    }

    // --- Jump logic ---
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
        if (d == 0) {
            Lcd_cursor(&lcd, 1, 0); Lcd_string(&lcd, "    ");
        }
        d = 1;
        Lcd_cursor(&lcd, 0, dino_col1); Lcd_write_char(&lcd, 2);
        Lcd_cursor(&lcd, 0, dino_col2); Lcd_write_char(&lcd, 3);
    } else {
        if (d == 1) {  // just landed
            Lcd_cursor(&lcd, 0, dino_col1); Lcd_string(&lcd, " ");
            Lcd_cursor(&lcd, 0, dino_col2); Lcd_string(&lcd, " ");
        }
        d = 0;
    }

    // --- Score timer ---
    if (now > timer3 + period3) {
        timer3 = now;
        score++;
        if (score == 100) {
            buzz_tone(800, 150);
            buzz_tone(900, 150);
            score = 0;
            score_hundreds++;
            if (score_hundreds == 100) score_hundreds = 0;
        }
        Lcd_cursor(&lcd, 1, 14); Lcd_int(&lcd, score);
        Lcd_cursor(&lcd, 0, 14); Lcd_int(&lcd, score_hundreds);

        btn_curr = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
        if (btn_curr != btn_prev) {
            Lcd_cursor(&lcd, 0, 1); Lcd_string(&lcd, "  ");
        }
        btn_prev = btn_curr;
    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA6 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB4 PB5 PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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

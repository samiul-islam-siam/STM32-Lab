/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Bare-metal header for dino game on STM32F446RE (HSI 16 MHz)
  *
  * Author: Md. Samiul Islam Siam
  *         Partho Kumar Mondal
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Bare-metal CMSIS header ── */
#include "stm32f446xx.h"
#include <stdint.h>

/* ── GPIO pin bit-mask aliases ── */
#define GPIO_PIN_0   ((uint16_t)0x0001U)
#define GPIO_PIN_1   ((uint16_t)0x0002U)
#define GPIO_PIN_2   ((uint16_t)0x0004U)
#define GPIO_PIN_3   ((uint16_t)0x0008U)
#define GPIO_PIN_4   ((uint16_t)0x0010U)
#define GPIO_PIN_5   ((uint16_t)0x0020U)
#define GPIO_PIN_6   ((uint16_t)0x0040U)
#define GPIO_PIN_7   ((uint16_t)0x0080U)
#define GPIO_PIN_8   ((uint16_t)0x0100U)
#define GPIO_PIN_9   ((uint16_t)0x0200U)
#define GPIO_PIN_10  ((uint16_t)0x0400U)
#define GPIO_PIN_11  ((uint16_t)0x0800U)
#define GPIO_PIN_12  ((uint16_t)0x1000U)
#define GPIO_PIN_13  ((uint16_t)0x2000U)
#define GPIO_PIN_14  ((uint16_t)0x4000U)
#define GPIO_PIN_15  ((uint16_t)0x8000U)

/* GPIO state constants */
#define GPIO_PIN_RESET  0U
#define GPIO_PIN_SET    1U

/* ── SysTick millisecond counter ───────────────────────────────────────────
 *  Declared here, defined in main.c, incremented in SysTick_Handler.
 * ─────────────────────────────────────────────────────────────────────────── */
extern volatile uint32_t sys_tick;

/* ── Inline GPIO read helper ───────────────────────────────────────────────
 *  Returns GPIO_PIN_SET (1) or GPIO_PIN_RESET (0).
 * ─────────────────────────────────────────────────────────────────────────── */
static inline uint8_t GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->IDR & (uint32_t)pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

/* ── Function prototypes ────────────────────────────────────────────────── */
void SystemClock_Config(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

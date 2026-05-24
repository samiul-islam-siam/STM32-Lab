#ifndef MAIN_H_
#define MAIN_H_

#include "stm32f446xx.h"   /* CMSIS device header – provides RCC, GPIO structs */
#include <stdint.h>

/* Re-use familiar pin-name macros */
#define GPIO_PIN_0   (1U <<  0)
#define GPIO_PIN_1   (1U <<  1)
#define GPIO_PIN_2   (1U <<  2)
#define GPIO_PIN_3   (1U <<  3)
#define GPIO_PIN_4   (1U <<  4)
#define GPIO_PIN_5   (1U <<  5)
#define GPIO_PIN_6   (1U <<  6)
#define GPIO_PIN_7   (1U <<  7)
#define GPIO_PIN_8   (1U <<  8)
#define GPIO_PIN_9   (1U <<  9)
#define GPIO_PIN_10  (1U << 10)
#define GPIO_PIN_11  (1U << 11)
#define GPIO_PIN_12  (1U << 12)
#define GPIO_PIN_13  (1U << 13)
#define GPIO_PIN_14  (1U << 14)
#define GPIO_PIN_15  (1U << 15)

void Error_Handler(void);

#endif /* MAIN_H_ */

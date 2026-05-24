/*
 * lcd.h – Bare-metal LCD 1602A driver, STM32F446RE
 * Mirrors the HAL version's public API exactly.
 */

#ifndef LCD_H_
#define LCD_H_

#include "stm32f446xx.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* #define LCD20xN */
#define LCD16xN

extern const uint8_t ROW_16[];
extern const uint8_t ROW_20[];

/* --------------------------------------------------------------------------
 * LCD command constants (unchanged from HAL version)
 * -------------------------------------------------------------------------- */
#define CLEAR_DISPLAY           0x01
#define RETURN_HOME             0x02

#define ENTRY_MODE_SET          0x04
#define OPT_S                   0x01
#define OPT_INC                 0x02

#define DISPLAY_ON_OFF_CONTROL  0x08
#define OPT_D                   0x04
#define OPT_C                   0x02
#define OPT_B                   0x01

#define CURSOR_DISPLAY_SHIFT    0x10
#define OPT_SC                  0x08
#define OPT_RL                  0x04

#define FUNCTION_SET            0x20
#define OPT_DL                  0x10
#define OPT_N                   0x08
#define OPT_F                   0x04
#define SETCGRAM_ADDR           0x40
#define SET_DDRAM_ADDR          0x80

/* --------------------------------------------------------------------------
 * Bare-metal GPIO write – replaces HAL_GPIO_WritePin
 * Uses BSRR: upper 16 bits reset, lower 16 bits set.
 * -------------------------------------------------------------------------- */
static inline void GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, uint8_t state)
{
    if (state)
        port->BSRR = (uint32_t)pin;          /* set */
    else
        port->BSRR = (uint32_t)pin << 16;    /* reset */
}

/* --------------------------------------------------------------------------
 * Bare-metal delay – replaces HAL_Delay
 * HSI = 16 MHz → ~16000 cycles per ms (conservative, no pipeline tricks)
 * -------------------------------------------------------------------------- */
void delay_ms(uint32_t ms);

/* --------------------------------------------------------------------------
 * Reuse HAL-style DELAY macro so lcd.c is unchanged
 * -------------------------------------------------------------------------- */
#define DELAY(X) delay_ms(X)

/* --------------------------------------------------------------------------
 * Type aliases (identical names to HAL version)
 * -------------------------------------------------------------------------- */
#define Lcd_PortType  GPIO_TypeDef*
#define Lcd_PinType   uint16_t

#define LCD_NIB          4
#define LCD_BYTE         8
#define LCD_DATA_REG     1
#define LCD_COMMAND_REG  0

typedef enum {
    LCD_4_BIT_MODE,
    LCD_8_BIT_MODE
} Lcd_ModeTypeDef;

typedef struct {
    Lcd_PortType *data_port;
    Lcd_PinType  *data_pin;
    Lcd_PortType  rs_port;
    Lcd_PinType   rs_pin;
    Lcd_PortType  en_port;
    Lcd_PinType   en_pin;
    Lcd_ModeTypeDef mode;
} Lcd_HandleTypeDef;

/* --------------------------------------------------------------------------
 * Public API – identical signatures to HAL version
 * -------------------------------------------------------------------------- */
void              Lcd_init(Lcd_HandleTypeDef *lcd);
void              Lcd_int(Lcd_HandleTypeDef *lcd, int number);
void              Lcd_string(Lcd_HandleTypeDef *lcd, char *string);
void              Lcd_cursor(Lcd_HandleTypeDef *lcd, uint8_t row, uint8_t col);
Lcd_HandleTypeDef Lcd_create(Lcd_PortType port[], Lcd_PinType pin[],
                              Lcd_PortType rs_port, Lcd_PinType rs_pin,
                              Lcd_PortType en_port, Lcd_PinType en_pin,
                              Lcd_ModeTypeDef mode);
void              Lcd_define_char(Lcd_HandleTypeDef *lcd, uint8_t code, uint8_t bitmap[]);
void              Lcd_clear(Lcd_HandleTypeDef *lcd);

#endif /* LCD_H_ */

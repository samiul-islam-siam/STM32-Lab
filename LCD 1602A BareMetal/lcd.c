/*
 * lcd.c – Bare-metal LCD 1602A driver, STM32F446RE
 * Drop-in replacement for the HAL version.
 * Only difference: HAL_GPIO_WritePin → GPIO_WritePin, HAL_Delay → delay_ms
 
 Autho: Md. Samiul Islam Siam 
        & Partho Kumar Mondal
 */

#include "lcd.h"

const uint8_t ROW_16[] = { 0x00, 0x40, 0x10, 0x50 };
const uint8_t ROW_20[] = { 0x00, 0x40, 0x14, 0x54 };

/* --------------------------------------------------------------------------
 * Delay implementation
 * At HSI 16 MHz, one loop iteration ≈ 4 cycles → 4M iterations ≈ 1 s
 * Calibrated constant: 4000 iterations ≈ 1 ms (conservative, safe)
 * -------------------------------------------------------------------------- */
void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        for (volatile uint32_t j = 0; j < 4000; j++) { }
}

/************************************** Static declarations **************************************/

static void lcd_write_data(Lcd_HandleTypeDef *lcd, uint8_t data);
static void lcd_write_command(Lcd_HandleTypeDef *lcd, uint8_t command);
static void lcd_write(Lcd_HandleTypeDef *lcd, uint8_t data, uint8_t len);

/************************************** Function definitions **************************************/

Lcd_HandleTypeDef Lcd_create(
    Lcd_PortType port[], Lcd_PinType pin[],
    Lcd_PortType rs_port, Lcd_PinType rs_pin,
    Lcd_PortType en_port, Lcd_PinType en_pin,
    Lcd_ModeTypeDef mode)
{
    Lcd_HandleTypeDef lcd;

    lcd.mode      = mode;
    lcd.en_pin    = en_pin;
    lcd.en_port   = en_port;
    lcd.rs_pin    = rs_pin;
    lcd.rs_port   = rs_port;
    lcd.data_pin  = pin;
    lcd.data_port = port;

    Lcd_init(&lcd);
    return lcd;
}

void Lcd_init(Lcd_HandleTypeDef *lcd)
{
    if (lcd->mode == LCD_4_BIT_MODE)
    {
        lcd_write_command(lcd, 0x33);
        lcd_write_command(lcd, 0x32);
        lcd_write_command(lcd, FUNCTION_SET | OPT_N);   /* 4-bit, 2 lines */
    }
    else
    {
        lcd_write_command(lcd, FUNCTION_SET | OPT_DL | OPT_N);
    }

    lcd_write_command(lcd, CLEAR_DISPLAY);
    lcd_write_command(lcd, DISPLAY_ON_OFF_CONTROL | OPT_D);
    lcd_write_command(lcd, ENTRY_MODE_SET | OPT_INC);
}

void Lcd_int(Lcd_HandleTypeDef *lcd, int number)
{
    char buffer[11];
    sprintf(buffer, "%d", number);
    Lcd_string(lcd, buffer);
}

void Lcd_string(Lcd_HandleTypeDef *lcd, char *string)
{
    for (uint8_t i = 0; i < strlen(string); i++)
        lcd_write_data(lcd, string[i]);
}

void Lcd_cursor(Lcd_HandleTypeDef *lcd, uint8_t row, uint8_t col)
{
#ifdef LCD20xN
    lcd_write_command(lcd, SET_DDRAM_ADDR + ROW_20[row] + col);
#endif
#ifdef LCD16xN
    lcd_write_command(lcd, SET_DDRAM_ADDR + ROW_16[row] + col);
#endif
}

void Lcd_clear(Lcd_HandleTypeDef *lcd)
{
    lcd_write_command(lcd, CLEAR_DISPLAY);
}

void Lcd_define_char(Lcd_HandleTypeDef *lcd, uint8_t code, uint8_t bitmap[])
{
    lcd_write_command(lcd, SETCGRAM_ADDR + (code << 3));
    for (uint8_t i = 0; i < 8; i++)
        lcd_write_data(lcd, bitmap[i]);
}

/************************************** Static function definitions **************************************/

static void lcd_write_command(Lcd_HandleTypeDef *lcd, uint8_t command)
{
    GPIO_WritePin(lcd->rs_port, lcd->rs_pin, LCD_COMMAND_REG);

    if (lcd->mode == LCD_4_BIT_MODE)
    {
        lcd_write(lcd, command >> 4,    LCD_NIB);
        lcd_write(lcd, command & 0x0F, LCD_NIB);
    }
    else
    {
        lcd_write(lcd, command, LCD_BYTE);
    }
}

static void lcd_write_data(Lcd_HandleTypeDef *lcd, uint8_t data)
{
    GPIO_WritePin(lcd->rs_port, lcd->rs_pin, LCD_DATA_REG);

    if (lcd->mode == LCD_4_BIT_MODE)
    {
        lcd_write(lcd, data >> 4,    LCD_NIB);
        lcd_write(lcd, data & 0x0F, LCD_NIB);
    }
    else
    {
        lcd_write(lcd, data, LCD_BYTE);
    }
}

static void lcd_write(Lcd_HandleTypeDef *lcd, uint8_t data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
        GPIO_WritePin(lcd->data_port[i], lcd->data_pin[i], (data >> i) & 0x01);

    GPIO_WritePin(lcd->en_port, lcd->en_pin, 1);
    DELAY(1);
    GPIO_WritePin(lcd->en_port, lcd->en_pin, 0);   /* latch on falling edge */
}

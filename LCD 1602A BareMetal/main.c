/**
  ******************************************************************************
  * @file    main.c
  * @brief   Bare-metal LCD 1602A – 4-bit parallel mode, STM32F446RE
  ******************************************************************************
  */

#include "main.h"
#include "lcd.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

int main(void)
{
    SystemClock_Config();
    MX_GPIO_Init();

    /* Pin mapping:
       D4 → PC7   D5 → PB6   D6 → PA7   D7 → PA6
       RS → PB5   EN → PB4                          */
    Lcd_PortType ports[] = { GPIOC, GPIOB, GPIOA, GPIOA };
    Lcd_PinType  pins[]  = { GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_6 };

    Lcd_HandleTypeDef lcd;
    lcd = Lcd_create(ports, pins,
                     GPIOB, GPIO_PIN_5,   /* RS */
                     GPIOB, GPIO_PIN_4,   /* EN */
                     LCD_4_BIT_MODE);

    Lcd_cursor(&lcd, 0, 1);
    Lcd_string(&lcd, "Siam & Partho");

    for (int x = 1; x <= 200; x++)
    {
        Lcd_cursor(&lcd, 1, 7);
        Lcd_int(&lcd, x);
        delay_ms(1000);
    }

    while (1) { }
}

/* --------------------------------------------------------------------------
 * System clock – HSI 16 MHz, no PLL
 * -------------------------------------------------------------------------- */
void SystemClock_Config(void)
{
    /* Enable HSI and wait until ready */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { }

    /* Select HSI as system clock source */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) { }

    /* AHB, APB1, APB2 prescalers all = 1 (reset default) */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
}

/* --------------------------------------------------------------------------
 * GPIO init
 * -------------------------------------------------------------------------- */
static void MX_GPIO_Init(void)
{
    /* Enable clocks for GPIOA, GPIOB, GPIOC */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                  | RCC_AHB1ENR_GPIOBEN
                  | RCC_AHB1ENR_GPIOCEN;

    /* Small delay after enabling clocks (AHB bus sync) */
    volatile uint32_t dummy = RCC->AHB1ENR; (void)dummy;

    /* Helper macro: set two MODER bits for a pin to "01" (output) */
    /* MODER field = bits [2n+1 : 2n] for pin n */

    /* PA6, PA7 → output */
    GPIOA->MODER &= ~((3U << (6*2)) | (3U << (7*2)));
    GPIOA->MODER |=  ((1U << (6*2)) | (1U << (7*2)));
    GPIOA->OTYPER  &= ~((1U << 6) | (1U << 7));   /* push-pull */
    GPIOA->OSPEEDR &= ~((3U << (6*2)) | (3U << (7*2))); /* low speed */
    GPIOA->PUPDR   &= ~((3U << (6*2)) | (3U << (7*2))); /* no pull */
    GPIOA->ODR     &= ~((1U << 6) | (1U << 7));   /* reset low */

    /* PB4, PB5, PB6 → output */
    GPIOB->MODER &= ~((3U << (4*2)) | (3U << (5*2)) | (3U << (6*2)));
    GPIOB->MODER |=  ((1U << (4*2)) | (1U << (5*2)) | (1U << (6*2)));
    GPIOB->OTYPER  &= ~((1U << 4) | (1U << 5) | (1U << 6));
    GPIOB->OSPEEDR &= ~((3U << (4*2)) | (3U << (5*2)) | (3U << (6*2)));
    GPIOB->PUPDR   &= ~((3U << (4*2)) | (3U << (5*2)) | (3U << (6*2)));
    GPIOB->ODR     &= ~((1U << 4) | (1U << 5) | (1U << 6));

    /* PC7 → output */
    GPIOC->MODER &= ~(3U << (7*2));
    GPIOC->MODER |=  (1U << (7*2));
    GPIOC->OTYPER  &= ~(1U << 7);
    GPIOC->OSPEEDR &= ~(3U << (7*2));
    GPIOC->PUPDR   &= ~(3U << (7*2));
    GPIOC->ODR     &= ~(1U << 7);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

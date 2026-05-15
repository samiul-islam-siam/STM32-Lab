/*
 * Morse Code Decoder using STM32F446RE
 *
 * USER Button : PC13
 * LED         : PA6
 * Buzzer      : PB1
 * UART2 TX    : PA2
 *
 * Press button:
 * Short press (<400ms)  -> DOT
 * Long press  (>=400ms) -> DASH
 *
 * After 1200ms idle:
 * Morse code is decoded
 * and printed on UART terminal.
 */

#include <stm32f446xx.h>
#include <stdint.h>
#include <string.h>

/* =========================================================
   GLOBAL TIMER COUNTER
   ========================================================= */

volatile uint32_t ms_count = 0;

/* =========================================================
   SYSTEM CLOCK = 180 MHz
   ========================================================= */

void SystemClock_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    PWR->CR |= PWR_CR_VOS;

    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    FLASH->ACR =
          FLASH_ACR_ICEN
        | FLASH_ACR_DCEN
        | FLASH_ACR_PRFTEN
        | FLASH_ACR_LATENCY_5WS;

    /*
     * PLL:
     * 16 MHz / 8 = 2 MHz
     * 2 MHz * 180 = 360 MHz
     * 360 / 2 = 180 MHz
     */

    RCC->PLLCFGR = 0;

    RCC->PLLCFGR |=
        (8U << RCC_PLLCFGR_PLLM_Pos);

    RCC->PLLCFGR |=
        (180U << RCC_PLLCFGR_PLLN_Pos);

    RCC->PLLCFGR |=
        (0U << RCC_PLLCFGR_PLLP_Pos);

    RCC->PLLCFGR |=
        RCC_PLLCFGR_PLLSRC_HSI;

    RCC->PLLCFGR |=
        (2U << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;

    while (!(RCC->CR & RCC_CR_PLLRDY));

    /*
     * Overdrive
     */

    PWR->CR |= PWR_CR_ODEN;
    while (!(PWR->CSR & PWR_CSR_ODRDY));

    PWR->CR |= PWR_CR_ODSWEN;
    while (!(PWR->CSR & PWR_CSR_ODSWRDY));

    /*
     * Bus clocks
     */

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;

    /*
     * Select PLL
     */

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS)
            != RCC_CFGR_SWS_PLL);
}

/* =========================================================
   USART2 INIT
   PA2 -> TX
   PA3 -> RX
   115200 baud
   ========================================================= */

void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /*
     * PA2 -> AF7
     */

    GPIOA->MODER &= ~(3U << (2 * 2));
    GPIOA->MODER |=  (2U << (2 * 2));

    GPIOA->AFR[0] &= ~(0xFU << (4 * 2));
    GPIOA->AFR[0] |=  (7U   << (4 * 2));

    /*
     * PA3 -> AF7
     */

    GPIOA->MODER &= ~(3U << (3 * 2));
    GPIOA->MODER |=  (2U << (3 * 2));

    GPIOA->AFR[0] &= ~(0xFU << (4 * 3));
    GPIOA->AFR[0] |=  (7U   << (4 * 3));

    /*
     * APB1 = 45 MHz
     * 115200 baud
     */

    USART2->BRR = (24U << 4) | 7U;

    USART2->CR1 =
          USART_CR1_TE
        | USART_CR1_RE
        | USART_CR1_UE;
}

/* =========================================================
   UART SEND
   ========================================================= */

void USART2_SendChar(char c)
{
    while (!(USART2->SR & USART_SR_TXE));

    USART2->DR = (uint8_t)c;
}

void USART2_SendString(const char *s)
{
    while (*s)
    {
        USART2_SendChar(*s++);
    }

    while (!(USART2->SR & USART_SR_TC));
}

/* =========================================================
   TIM2 -> 1ms interrupt
   TIM CLK = 90 MHz
   PSC = 89  -> 1 MHz
   ARR = 999 -> 1 ms
   ========================================================= */

void TIM2_Init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    __NOP();
    __NOP();

    TIM2->CR1 &= ~TIM_CR1_CEN;

    TIM2->PSC = 89U;

    TIM2->ARR = 999U;

    TIM2->EGR = TIM_EGR_UG;

    TIM2->SR = 0U;

    TIM2->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, 0U);

    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR &= ~TIM_SR_UIF;

        ms_count++;
    }
}

/* =========================================================
   DELAY
   ========================================================= */

void delay_ms(uint32_t ms)
{
    uint32_t start = ms_count;

    while ((ms_count - start) < ms);
}

/* =========================================================
   GPIO INIT
   PB0 -> LED
   PB1 -> BUZZER
   PC13 -> USER BUTTON
   ========================================================= */

void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    /*
     * PA6 = LED OUTPUT
     */

    GPIOA->MODER &= ~(3U << (6 * 2));

    GPIOA->MODER |=  (1U << (6 * 2));

    /*
     * PB1 = OUTPUT
     */

    GPIOB->MODER &= ~(3U << (1 * 2));

    GPIOB->MODER |=  (1U << (1 * 2));

    /*
     * PC13 = INPUT
     */

    GPIOC->MODER &= ~(3U << (13 * 2));
}

/* =========================================================
   OUTPUT CONTROL
   ========================================================= */

void OUTPUT_ON(void)
{
    GPIOA->BSRR = (1UL << 6); //For led
}

void OUTPUT_OFF(void)
{
    GPIOA->BSRR = (1UL << (6+16)); //For led

}

/* =========================================================
   MORSE TABLE
   ========================================================= */

typedef struct
{
    const char *morse;

    char alpha;

} Morse;

Morse morseTable[] =
{
    {".-",    'A'},
    {"-...",  'B'},
    {"-.-.",  'C'},
    {"-..",   'D'},
    {".",     'E'},
    {"..-.",  'F'},
    {"--.",   'G'},
    {"....",  'H'},
    {"..",    'I'},
    {".---",  'J'},
    {"-.-",   'K'},
    {".-..",  'L'},
    {"--",    'M'},
    {"-.",    'N'},
    {"---",   'O'},
    {".--.",  'P'},
    {"--.-",  'Q'},
    {".-.",   'R'},
    {"...",   'S'},
    {"-",     'T'},
    {"..-",   'U'},
    {"...-",  'V'},
    {".--",   'W'},
    {"-..-",  'X'},
    {"-.--",  'Y'},
    {"--..",  'Z'}
};

/* =========================================================
   MORSE DECODE
   ========================================================= */

char DecodeMorse(char *code)
{
    for (int i = 0; i < 26; i++)
    {
        if (strcmp(code,
                   morseTable[i].morse) == 0)
        {
            return morseTable[i].alpha;
        }
    }

    return '?';
}

/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    SystemClock_Config();

    USART2_Init();

    TIM2_Init();

    GPIO_Init();

    USART2_SendString(
        "\r\n=== Morse Decoder Ready ===\r\n");

    char morseBuffer[10];

    uint8_t idx = 0;

    /*
     * USER BUTTON:
     * released = 1
     * pressed  = 0
     */

    uint8_t prevButton = 1;

    uint32_t pressStart = 0;

    uint32_t releaseTime = 0;
    uint8_t sound_on = 0;

    while (1)
    {
        uint8_t button =
            (GPIOC->IDR & GPIO_IDR_ID13) ? 1 : 0;

        /*
         * BUTTON PRESSED
         */

        if (button == 0 && prevButton == 1)
        {
            delay_ms(20);

            button =
                (GPIOC->IDR & GPIO_IDR_ID13) ? 1 : 0;

            if (button == 0 && sound_on == 0)
            {
                pressStart = ms_count;
                sound_on = 1;
                OUTPUT_ON();
            }
        }
        /*
         * BUTTON RELEASED
         */

        if (button == 1 && prevButton == 0)
        {
            delay_ms(20);

            button =
                (GPIOC->IDR & GPIO_IDR_ID13) ? 1 : 0;

            if (button == 1 && sound_on == 1)
            {
            	sound_on = 0;
                OUTPUT_OFF();

                uint32_t duration =
                    ms_count - pressStart;

                if (duration < 400)
                {
                    morseBuffer[idx++] = '.';

                    USART2_SendString(".");
                }
                else
                {
                    morseBuffer[idx++] = '-';

                    USART2_SendString("-");
                }

                morseBuffer[idx] = '\0';

                releaseTime = ms_count;
            }
        }

        if (sound_on)
        {
            static uint32_t last_toggle = 0;

            if (ms_count - last_toggle >= 1)
            {
                GPIOB->ODR ^= (1 << 1);
                last_toggle = ms_count;
            }
        }

        /*
         * END OF CHARACTER
         */

        if (idx > 0 &&
            (ms_count - releaseTime) > 1200)
        {
            char decoded =
                DecodeMorse(morseBuffer);

            USART2_SendString(" -> ");

            USART2_SendChar(decoded);

            USART2_SendString("\r\n");

            idx = 0;

            memset(morseBuffer,
                   0,
                   sizeof(morseBuffer));
        }

        prevButton = button;
    }
}

#include "stm32f446xx.h"

void delay(uint32_t d)
{
    while(d--);
}

int main(void)
{
    /* Enable GPIOA clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* Small delay for clock stabilization */
    __NOP(); __NOP();

    /* Set PA6 as Output */
    GPIOA->MODER &= ~(3UL << (6 * 2));   // Clear bits
    GPIOA->MODER |=  (1UL << (6 * 2));   // Set as Output (01)

    /* Push-pull (default) */
    GPIOA->OTYPER &= ~(1UL << 6);

    /* No pull-up/down */
    GPIOA->PUPDR &= ~(3UL << (6 * 2));

    while(1)
    {
        /* LED ON */
        GPIOA->BSRR = (1UL << 6);

        delay(500000);

        /* LED OFF */
        GPIOA->BSRR = (1UL << (6 + 16));

        delay(500000);
    }
}

// -------- Hardware Connections ---------
Anode D12 & Cathod Groud

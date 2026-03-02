#include <stdint.h>

// RCC AHB1 Peripheral Clock Enable Register
#define RCC_AHB1ENR \
(*(volatile uint32_t*)0x40023830)

// GPIOA Mode Register
#define GPIOA_MODER \
(*(volatile uint32_t*)0x40020000)

// GPIOA Bit Set/Reset Register (BSRR)
#define GPIOA_BSRR \
(*(volatile uint32_t*)0x40020018)


void delay(volatile uint32_t n) {
    while (n--);
}

int main(void) {

    // Enable GPIOA clock (bit 0)
    RCC_AHB1ENR |= (1 << 0);

    /*
       Configure PA5 as output
       Pin 5 → bits 11:10 in MODER
    */
    GPIOA_MODER &= ~(3 << 10);  // Clear bits 11:10
    GPIOA_MODER |=  (1 << 10);  // Set as Output (01)

    while (1) {

        /* Set PA5 HIGH (LED ON)
           Lower 16 bits of BSRR set pins
        */
        GPIOA_BSRR = (1 << 5);

        delay(500000);

        /* Reset PA5 LOW (LED OFF)
           Upper 16 bits reset pins
           So bit position = 5 + 16 = 21
        */
        GPIOA_BSRR = (1 << (5 + 16));

        delay(500000);
    }
}

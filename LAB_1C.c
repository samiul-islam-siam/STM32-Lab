#include <stdint.h>

#define RCC_AHB1ENR \
(*(volatile uint32_t*)0x40023830)

#define GPIOA_MODER \
(*(volatile uint32_t*)0x40020000)

#define GPIOA_ODR \
(*(volatile uint32_t*)0x40020014)

void delay(volatile uint32_t n) {
	while (n--);
}

int main(void) {
	RCC_AHB1ENR |= (1 << 0); // GPIOA clk
	GPIOA_MODER &= ~(3 << 10); // Clear PA5
	GPIOA_MODER |= (1 << 10); // Output
	while (1) {
		GPIOA_ODR ^= (1 << 5); // Toggle PA5
		delay(500000);
	}
}

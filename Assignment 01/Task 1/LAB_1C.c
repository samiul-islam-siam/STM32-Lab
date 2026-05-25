#include <stdint.h>

/*
  RCC_AHB1ENR Register Address: 0x40023830
  This register controls enabling clocks for AHB1 peripherals.
  Bit 0 = GPIOA clock enable
*/

// 'volatile' prevents compiler optimization.

#define RCC_AHB1ENR \
(*(volatile uint32_t*)0x40023830)

//  GPIOA_MODER Register : This register sets the mode of GPIOA pins.
#define GPIOA_MODER \
(*(volatile uint32_t*)0x40020000)

/*
  GPIOA_ODR Register Address: 0x40020014
  Output Data Register.
  Each bit corresponds to one GPIO pin.
  Bit 5 controls PA5.
*/

#define GPIOA_ODR \
(*(volatile uint32_t*)0x40020014)

void delay(volatile uint32_t n) {
	while (n--);
}

int main(void) {

	//  Setting Bit 0, it enables GPIOA clock.
	RCC_AHB1ENR |= (1 << 0); // GPIOA clk

	/*
       PA5 uses bits 11:10 in MODER register.
       Because each pin uses 2 bits: pin_number × 2 = bit position
       5 × 2 = 10
       So bits 10 and 11 control PA5 mode.
    */

    /*
       First clear bits 11:10 (make them 00)
	   and then Set bit 10 to 1 (01 = output mode)
    */
	GPIOA_MODER &= ~(3 << 10); // Clear PA5
	GPIOA_MODER |= (1 << 10); // Output mode 
	
	while (1) {
		GPIOA_ODR ^= (1 << 5); // Toggle PA5
		delay(500000);
	}
}

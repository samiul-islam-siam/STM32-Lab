# WS2812B LED Control 

Implementations of WS2812B RGB LED strip control using the STM32F446RE.

This folder contains:
- HAL + PWM + DMA based implementation
- Bare-metal register-level implementation using NOP


## Features

- WS2812B LED control
- Multiple LED support
- Brightness control
- HSV to RGB conversion
- Color animations
- UART debug output
- Timing-accurate bare-metal driver



## Pin Connection

| STM32F446RE | WS2812B |
|---|---|
| PA8 | DIN |
| GND | GND |
| 5V | VCC |



## HAL + DMA Version

The HAL implementation uses:
- TIM1 PWM
- DMA transfer
- PWM duty cycle encoding

### Example

```c
Set_LED(0, 210, 105, 30);
Set_LED(1, 0, 90, 0);
Set_LED(2, 0, 0, 200);
Set_LED(3, 200, 200, 0);
Set_LED(4, 165, 42, 42);

Set_Brightness(10);
WS2812B_Send();
```


## Bare-Metal Version

The bare-metal implementation uses:
- Direct GPIO register access
- NOP-based timing
- 16MHz HSI clock
- Bit-banging method



## Demonstrations

- Static color cycle
- HSV hue sweep
- LED chase animation


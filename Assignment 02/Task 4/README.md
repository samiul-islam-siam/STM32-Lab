# Task 4 — PWM Generation and LED Brightness Control

## Goal

Generate a **1 kHz PWM signal** on TIM3 CH1 (PA6 / Arduino D12) to control LED brightness in three parts:

1. Duty-cycle sweep 0% → 100% in 10% steps (300 ms/step)
2. Sine-wave breathing effect via a 256-entry LUT (5 full cycles)
3. Final hold at 50% duty cycle

## About the LED

A standard **single colour LED** is used as the brightness-controlled output. The PWM signal switches the LED at 1 kHz — faster than the human eye can perceive. 
Therefore, the perceived brightness tracks the duty cycle linearly (0% = off, 100% = full brightness). The sine LUT (Look Up Table) creates a smooth, natural-looking breathing effect.

<p style="text-align: center;">
  <img src="/images/LED-Construction.png" width="40%" alt="front and back">
</p>

**Typical applications:** status indicators, backlight control, ambient lighting dimmers, breathing/heartbeat UI feedback.

## Hardware List

| # | Component                               | Qty |
|---|-----------------------------------------|-----|
| 1 | NUCLEO-F446RE development board         | 1   |
| 2 | Single colour LED                       | 1   |
| 3 | 220 Ω – 330 Ω current-limiting resistor | 1   |

## Circuit Connections

```
PA6 ──[220 Ω]────  LED Anode   (longer leg  / +)
GND ─────────────  LED Cathode (shorter leg / –)
```
 
> PA6 is labelled as **D12** on the Arduino Uno header of the NUCLEO-64.

> ⚠️ **Always use a current-limiting resistor** (220 Ω – 330 Ω). Connecting an LED directly without a resistor may damage the LED and the GPIO pin.

## PWM Configuration

### Clock Setup

```
TIM3_CLK = 90 MHz   (APB1 timer clock on STM32F446 @ 180 MHz system clock)
PSC      = 89       →  tick frequency = 90 MHz / (89 + 1) = 1 MHz  (1 µs/tick)
ARR      = 999      →  period = 1000 ticks = 1 ms  →  1 kHz PWM
```

### Duty Cycle Formula

```
CCR1 = pct × (ARR + 1) / 100
     = pct × 1000      / 100
     = pct × 10

here, pct = percentage duty cycle

Examples:
   0%  →  CCR1 =    0
  50%  →  CCR1 =  500
 100%  →  CCR1 = 1000
```

## Sine LUT Breathing Effect

A 256-entry lookup table maps step index `i` to a duty-cycle percentage:

```
LUT[i] = 50 × (1 + sin(2π × i / 256)) where i = 0, 1, 2, ..., 255

Range:  0% (sin = –1)  →  50% (sin = 0)  →  100% (sin = +1)
Period: 256 steps × 8 ms/step = 2048 ms ≈ 2 s per breath cycle
```

The LUT is pre-computed at startup using the FPU-accelerated `sinf()`. The FPU must be enabled before any floating-point instruction.

## Serial Output

<p style="text-align: center;">
  <img src="/images/task4_output.png" width="50%" alt="output">
</p>

## Oscilloscope Output

<p style="text-align: center;">
  <img src="/images/T4_PWM_25.jpg" width="30%" alt="output">
  <img src="/images/T4_PWM_50.jpg" width="30%" alt="output">
  <img src="/images/T4_PWM_75.jpg" width="30%" alt="output">
</p>
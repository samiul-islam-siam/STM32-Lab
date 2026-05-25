# Task 5 — WS2812B RGB LED: Colour Mixing and Animation

## Goal

Drive a strip of **5 WS2812B individually addressable RGB LEDs** using TIM1 PWM + DMA2, demonstrating:
1. A 10-colour static palette (1 s per colour)
2. A full hue sweep 0°→359° (3°/step, 25 ms/step)
3. A red colour-chase animation across all 5 LEDs (3 rounds)

## About the WS2812B

The WS2812B is a **self-contained addressable RGB LED**, controlled over a single-wire NZR (Non-Return-to-Zero) protocol. Each LED daisy-chains data to the next, enabling full independent colour control over a single GPIO line.

<p style="text-align: center;">
  <img src="/images/ws2812b_rgb.png" width="50%" alt="front and back">
</p>

| Parameter         | Value                               |
|-------------------|-------------------------------------|
| Supply voltage    | 3.5 V – 5.3 V                       |
| Logic input (DIN) | 3.3 V / 5 V compatible              |
| Protocol          | Single-wire NZR, 800 kHz            |
| Data order        | GRB (Green → Red → Blue), MSB first |
| Reset time        | ≥ 50 µs (line LOW)                  |
| Cascade           | Unlimited (Dout → Din chain)        |

**Typical applications:** LED strips, matrix displays, wearables, ambient lighting, indicator arrays.

> 📄 See [`WS2812B Datasheet`](WS2812B-LED-datasheet.pdf) for full electrical and timing specifications.

## Hardware List

| # | Component                         | Qty  |
|---|-----------------------------------|------|
| 1 | NUCLEO-F446RE development board   | 1    |
| 2 | WS2812B LED (individual or strip) | 5    |
| 5 | 5 V power supply (≥ 500 mA)       | 1    |
| 7 | Jumper wires                      | 3    |

## Circuit Connections

```
PA8 ─────────────  WS2812B  DIN   (Signal)
5 V ─────────────  WS2812B  VDD   (Power)
GND ─────────────  WS2812B  GND   (Ground)
```
<p style="text-align: center;">
  <img src="/images/ws2812b_diagram.jpg" width="50%" alt="front and back">
</p>

> PA8 is labelled **D7** on the Arduino Uno header of the NUCLEO-64.

> ⚠️ **Power the strip from an external 5 V supply.** The Nucleo's on-board 5 V or 3.3 V pin can also be used.

## Protocol & Timer Configuration

### NZR Bit Timing (800 kHz)

```
TIM1_CLK = 180 MHz
PSC      = 0         →  tick = 180 MHz  (1 tick ≈ 5.56 ns)
ARR      = 225       →  period = 226 ticks = 1.256 µs  →  ~796 kHz

Logic-1:  T1H = 144 ticks = 0.800 µs HIGH  |  T1L = 82 ticks = 0.456 µs LOW
Logic-0:  T0H =  72 ticks = 0.400 µs HIGH  |  T0L = 154 ticks = 0.856 µs LOW
Reset:    50 × zero CCR entries = 62.8 µs LOW
```

### Data Format

```
Each LED = 24 bits (GRB order, MSB first):
  [G7 G6 … G0] [R7 R6 … R0] [B7 B6 … B0]

PWM buffer size = (NUM_LEDS × 24) + WS_RESET
                = (5 × 24) + 50 = 170 × uint16_t entries
```

### DMA Transfer (Memory → Peripheral)

| Parameter   | Setting                                      |
|-------------|----------------------------------------------|
| DMA         | DMA2 Stream 1, Channel 6                     |
| Source      | `pwmData[]` (uint16_t, 170 entries)          |
| Destination | `TIM1->CCR1` (fixed)                         |
| Width       | 16-bit memory / 16-bit peripheral            |
| Mode        | Single (non-circular), polling on TC flag    |
| Trigger     | TIM1 CH1 CC1DE (capture/compare DMA request) |

> ⚠️ TIM1 is an **advanced-control timer** — `TIM1->BDTR |= TIM_BDTR_MOE` (**Main Output Enable**) is mandatory, or CH1 output remains LOW regardless of CCR1.

### Hue Sweep Formula

```
seg  = H / 60                 (integer sector 0–5)
frac = H % 60
q    = 255 × (60 – frac) / 60    (falling ramp)
t    = 255 × frac / 60            (rising ramp)

Sector 0: R=255, G=t,   B=0
Sector 1: R=q,   G=255, B=0
Sector 2: R=0,   G=255, B=t
Sector 3: R=0,   G=q,   B=255
Sector 4: R=t,   G=0,   B=255
Sector 5: R=255, G=0,   B=q
```

## Serial Output

<p style="text-align: center;">
  <img src="/images/task5_output.png" width="50%" alt="front and back">
</p>

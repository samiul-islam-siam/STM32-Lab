# Task 6B — Servo Motor Control

## Goal

Drive an **MG996R servo motor** from **0° → 180°** in 10° steps using 50 Hz PWM on TIM2 CH1 (PA0 / Arduino A0), then park at 90°. All angle-to-pulse math is done with FPU-accelerated floating-point.

## About the MG996R

The MG996R is a **high-torque, metal-gear PWM servo** commonly used in robotics, RC vehicles, and pan-tilt camera mounts.

<p style="text-align: left;">
  <img src="/images/mg996r_components.jpg" width="40%" alt="mg966r">
</p>

| Parameter            | Value                             |
|----------------------|-----------------------------------|
| Operating voltage    | 4.8 V – 7.2 V                     |
| Stall torque (4.8 V) | 9.4 kgf·cm                        |
| Control signal       | 50 Hz PWM, 3.3 V / 5 V compatible |
| Pulse range          | ~500 µs (0°) – ~2500 µs (180°)    |
| Connector            | JR / Futaba 3-pin                 |

**Typical applications:** robotic arms, steering servos, pan-tilt gimbals, door/valve actuators.

> 📄 See [`MG996R Datasheet`](MG996R_Tower-Pro.pdf) for full electrical specifications.

## Hardware List

| # | Component                         | Qty |
|---|-----------------------------------|-----|
| 1 | NUCLEO-F446RE development board   | 1   |
| 2 | MG996R servo motor                | 1   |
| 5 | Jumper wires (M-M / M-F)          | 3   |

## Circuit Connections and Signal

```
PA0  ───────── MG996R  Orange wire  (PWM)
5 V  ───────── MG996R  Red wire     (VCC)
GND  ───────── MG996R  Brown wire   (GND)
```
<p style="text-align: center;">
  <img src="/images/mg996r_signal.png" width="50%" alt="signal">
</p>

> PA0 is labelled **A0** on the Arduino Uno header of the NUCLEO-64.

> ⚠️ **Power the servo from an external 5 V supply.** The Nucleo's on-board 5 V pin can also be used but do NOT power from Nucleo 3.3 V pin.  
> ⚠️ **Always share GND** between the Nucleo and the servo power supply, or the PWM signal will float.

## PWM Configuration

### Clock Setup

```
TIM2_CLK = 90 MHz   (APB1 timer clock on STM32F446RE @ 180 MHz system clock)
PSC      = 99       →  tick frequency = 90 MHz / (99 + 1) = 900 kHz
ARR      = 17999    →  period = 18 000 ticks / 900 kHz    = 20 ms  →  50 Hz
```

### Pulse Width Mapping

```
Angle  → Pulse Width  →  CCR1 Value:
   0°  →      500 µs  →  CCR1 =  450   ( 500 × 0.9)
  90°  →     1500 µs  →  CCR1 = 1350   (1500 × 0.9)
 180°  →     2500 µs  →  CCR1 = 2250   (2500 × 0.9)
```
```
Formula:
  t_us  = 500 + (angle / 180) × 2000 [µs]
  CCR1  = t_us × 0.9
  
  here, factor 0.9 = tick_freq (900 kHz) / 1 MHz
```

## Serial Output

<p style="text-align: center;">
  <img src="/images/task6b_output.png" width="50%" alt="front and back">
</p>
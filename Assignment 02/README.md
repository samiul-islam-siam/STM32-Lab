# CSE 2206: Lab-02

## Table of Contents
- [Hardware Platform](#hardware-platform)
- [Clock Configuration](#clock-configuration)
- [Task Overview](#task-overview)
- [Task 2 — Delay Generation](#task-2--delay-generation)
- [Task 3 — Code Profiling](#task-3--code-profiling)
- [Task 4 — PWM LED Control](#task-4--pwm-led-control)
- [Task 5 — WS2812B RGB LEDs](#task-5--ws2812b-rgb-leds)
- [Task 6A — Passive Buzzer](#task-6a--passive-buzzer)
- [Task 6B — Servo Motor](#task-6b--servo-motor)
- [Task 6C — Input Capture](#task-6c--input-capture)
- [Shared Driver Reference](#shared-driver-reference)
- [Building & Flashing](#building--flashing)

## Hardware Platform

| Item | Detail |
|------|--------|
| MCU | STM32F446RE (ARM Cortex-M4F, up to 180 MHz) |
| Board | NUCLEO-64 |
| Onboard LED | LD2 — PA5 |
| USART2 TX | PA2 → ST-Link Virtual COM Port |
| USART2 RX | PA3 |
| Serial Settings | 115200 baud · 8N1 |
| Debug | ST-Link V2-1 (onboard) |

## Clock Configuration

```
HSI (16 MHz)
  └─► PLLM (/8) → 2 MHz
        └─► PLLN (×180) → 360 MHz VCO
              └─► PLLP (/2) → 180 MHz SYSCLK
                    ├─ AHB  /1  → 180 MHz  (HCLK)
                    ├─ APB1 /4  →  45 MHz  → Timer clock = 90 MHz
                    └─ APB2 /2  →  90 MHz  → Timer clock = 180 MHz
```

> **Over-Drive mode** is mandatory for 180 MHz (`PWR_CR_ODEN` + `PWR_CR_ODSWEN`).  
> **Flash latency** must be set to **5 wait states** before switching to PLL.

## Task Overview

| Task | Description | Key Peripheral | Output Pin |
|------|-------------|----------------|------------|
| 2 | Delay Generation | TIM6 (1 µs tick) | PA5 (LED) |
| 3 | Code Profiling | DWT + TIM2 (32-bit) | USART2 |
| 4 | PWM LED Control | TIM3 CH1 | PA6 |
| 5 | WS2812B RGB LEDs | TIM1 + DMA2 | PA8 |
| 6A | Passive Buzzer | TIM4 CH1 | PB6 |
| 6B | Servo Motor | TIM2 CH1 | PA0 |
| 6C | Input Capture | TIM5 CH1/CH2 | PA0 (in) |

## Task 2 — Delay Generation

**Goal:** Implement µs / ms / s / HMS blocking delays using TIM6.

### Timer Setup
```
TIM6_CLK = 90 MHz
PSC = 89  →  tick = 1 MHz  →  1 tick = 1 µs
ARR = 0xFFFF  →  free-running, wraps every 65.535 ms
```

### Key Functions

| Function | Signature | Notes |
|----------|-----------|-------|
| `TIM6_Init` | `void TIM6_Init(void)` | PSC=89, ARR=0xFFFF, CEN=1 |
| `delay_us` | `void delay_us(uint16_t us)` | 16-bit wrap-safe timestamp subtraction |
| `delay_ms` | `void delay_ms(uint32_t ms)` | loops `delay_us(1000)` |
| `delay_s` | `void delay_s(uint32_t sec)` | loops `delay_ms(1000)` |
| `delay_hms` | `void delay_hms(uint8_t h, uint8_t m, uint8_t s)` | converts to seconds |

### Wrap-Safe Delay Core
```c
uint16_t start = TIM6->CNT;
while ((uint16_t)(TIM6->CNT - start) < us) {}
// unsigned 16-bit subtraction handles counter overflow automatically
```

### Circuit Connections
```
NUCLEO-64
  PA5 ──[built-in LED LD2]── GND   (no external wiring needed)
```

## Task 3 — Code Profiling

**Goal:** Measure execution time of 5 code blocks using two independent methods.

### Method 1 — DWT Cycle Counter
```
CoreDebug->DEMCR |= TRCENA   // master enable
DWT->CYCCNT = 0              // reset
DWT->CTRL   |= CYCCNTENA     // start counting

Resolution: 1 / 180 MHz = 5.56 ns per count
Overflow after: 2³² / 180e6 ≈ 23.9 seconds
```

### Method 2 — TIM2 Free-Running (32-bit, 1 µs)
```
TIM2_CLK = 90 MHz
PSC = 89  →  1 µs tick
ARR = 0xFFFFFFFF  →  overflows after 4295 s
```

### PROFILE Macro
```c
#define PROFILE(num, label, block)          \
    do {                                    \
        uint32_t _t0 = DWT_GetCycles();     \
        uint32_t _u0 = TIM2_GetMicros();    \
        { block }                           \
        uint32_t _cyc = DWT_GetCycles()-_t0;\
        uint32_t _us  = TIM2_GetMicros()-_u0;\
        Profile_PrintRow(...);              \
    } while (0)
```

### Benchmarked Blocks

| # | Block | Expected range |
|---|-------|----------------|
| 1 | BubbleSort N=100 (worst case) | ~5 ms |
| 2 | `delay_ms(100)` | ~100 ms |
| 3 | `USART2_SendString` 48 B | ~4 ms |
| 4 | `isqrt()` × 1000 inputs | ~0.5 ms |
| 5 | `memcpy` byte × 512 B | ~0.05 ms |

### Circuit Connections
```
No external wiring — output via USART2 → USB to PC
```

## Task 4 — PWM LED Control

**Goal:** Generate 1 kHz PWM on PA6, sweep 0–100% duty, then sine-wave breathe.

### Timer Setup
```
TIM3_CLK = 90 MHz
PSC = 89   →  tick = 1 MHz
ARR = 999  →  period = 1000 µs  →  f = 1 kHz
CCR1 = duty% × (ARR+1) / 100 = duty% × 10
```

### PWM Mode 1
```
CNT < CCR1  →  PA6 HIGH
CNT ≥ CCR1  →  PA6 LOW
OC1PE = 1   →  CCR1 updates buffered until next overflow (glitch-free)
```

### Sine LUT
```c
// Pre-computed at startup:
sine_lut[i] = (uint8_t)(50.0f * (1.0f + sinf(2π × i / 256)) + 0.5f);
// Range: 0–100 (duty %)
// Step: 8 ms → full cycle = 256 × 8 = 2048 ms ≈ 2 s
```

### Circuit Connections
```
NUCLEO-64 PA6  ──[330 Ω]──┬── LED anode
                           └── (cathode to GND)

PA6: D12 on Arduino header
```

> ⚠️ **Enable FPU** before calling `sinf()`:  
> `SCB->CPACR |= (0xF << 20); __DSB(); __ISB();`

## Task 5 — WS2812B RGB LEDs

**Goal:** Drive a 5-LED WS2812B chain using TIM1 PWM + DMA2 for bit-accurate timing.

### Protocol Timing
```
Bit period  = 226 ticks / 180 MHz = 1.256 µs   (TIM1 PSC=0, ARR=225)
Logic 1     = CCR1 = 144  →  HIGH 0.80 µs / LOW 0.45 µs
Logic 0     = CCR1 =  72  →  HIGH 0.40 µs / LOW 0.85 µs
Reset pulse = 50 × CCR1=0  →  ≥62.8 µs continuous LOW
Data order  = GRB, MSB first
```

### DMA2 Configuration
```
Stream1, Channel 6  →  TIM1_CH1 DMA request
Direction : Memory → Peripheral
MSIZE/PSIZE : 16-bit (uint16_t CCR1 values)
MINC = 1  (auto-increment through pwmData[])
PINC = 0  (always writes to TIM1->CCR1)
```

### Transfer Sequence
```
1. Build pwmData[]: for each bit → T1H or T0H; append 50 zeros (reset)
2. Stop TIM1, disable DMA stream, clear flags
3. Set M0AR = pwmData, NDTR = NUM_LEDS×24 + 50
4. Reset TIM1 CNT and SR
5. Enable CC1DE (TIM1 DMA request)
6. Enable DMA stream, then start TIM1
7. Poll DMA2->LISR TCIF1
8. Cleanup: clear TCIF1, stop TIM1, disable DMA, clear CC1DE
```

> ⚠️ **BDTR MOE** (`TIM1->BDTR = TIM_BDTR_MOE`) is **mandatory** for TIM1 outputs.

### Circuit Connections
```
NUCLEO-64 PA8  ──────────────────────── WS2812B Din
5 V supply    ──[100 µF cap to GND]──── WS2812B VCC
GND           ───────────────────────── WS2812B GND

PA8: D7 on Arduino header
Optional: 300–500 Ω series resistor on Din to protect against ringing
```

## Task 6A — Passive Buzzer

**Goal:** Play C-major scale and "Twinkle Twinkle" melody on a passive buzzer via TIM4.

### Timer Setup
```
TIM4_CLK = 90 MHz
PSC = 99  →  tick = 900 kHz
ARR formula:  ARR = (900,000 / f_note) − 1
CCR1 = (ARR + 1) / 2   →  50% duty cycle (max volume for piezo)
```

### Note Table (C-major scale)

| Note | Freq (Hz) | ARR | f_actual (Hz) |
|------|-----------|-----|---------------|
| C4 | 261.6 | 3436 | 261.9 |
| D4 | 293.7 | 3061 | 293.9 |
| E4 | 329.6 | 2727 | 330.0 |
| F4 | 349.2 | 2572 | 349.8 |
| G4 | 392.0 | 2295 | 392.2 |
| A4 | 440.0 | 2044 | 440.1 |
| B4 | 493.9 | 1821 | 494.2 |
| C5 | 523.3 | 1717 | 524.0 |

### Delay Mechanism (TIM2 + NVIC)
```c
// TIM2: PSC=89 → 1 µs tick, ARR=999 → overflow every 1 ms
// ISR: TIM2_IRQHandler increments volatile ms_count
// delay_ms: uint32_t start = ms_count; while((ms_count - start) < ms){}
```

### Circuit Connections
```
NUCLEO-64 PB6  ──[100 Ω]──── Passive Buzzer (+)
GND            ───────────── Passive Buzzer (−)

PB6: D10 on Arduino header
Note: active buzzer will NOT work — it needs a varying frequency signal
```

## Task 6B — Servo Motor

**Goal:** Control MG996R servo angle 0°→180° in 10° steps using 50 Hz PWM.

### Timer Setup
```
TIM2_CLK = 90 MHz
PSC = 99   →  tick = 900 kHz
ARR = 17999  →  period = 18,000 ticks / 900 kHz = 20 ms  →  50 Hz
```

### Pulse Width Mapping
```
Angle → pulse (µs) → CCR1:
  0°   →  500 µs  →  CCR1 =  450   ( 500 × 0.9)
 90°   → 1500 µs  →  CCR1 = 1350   (1500 × 0.9)
180°   → 2500 µs  →  CCR1 = 2250   (2500 × 0.9)

Formula: CCR1 = (500 + angle/180 × 2000) × 0.9
Factor 0.9 = tick_freq(900 kHz) / 1 MHz
```

### Circuit Connections
```
NUCLEO-64 PA0  ───────── MG996R Orange wire (Signal)
5 V supply     ───────── MG996R Red wire   (VCC)
GND            ───────── MG996R Brown wire (GND)

PA0: A0 on Arduino header
⚠️  Power servo from external 5 V — do NOT power from Nucleo 3.3 V pin
⚠️  Share GND between Nucleo and servo supply
```

> ⚠️ **Enable FPU** before calling angle conversion:  
> `SCB->CPACR |= ((3UL<<20)|(3UL<<22)); __DSB(); __ISB();`

## Task 6C — Input Capture

**Goal:** Measure frequency and duty cycle of a 1 kHz TIM3 signal using TIM5 dual-edge capture.

### Signal Generator (TIM3 → PA6)
```
PSC=89, ARR=999  →  1 kHz
CCR1 = duty% × 10
```

### Input Capture (TIM5 → PA0)
```
TIM5_CLK = 90 MHz, PSC=89  →  1 µs tick, ARR=0xFFFFFFFF
CH1 (CC1S=01, CC1P=0) → captures RISING  edges → stores in CCR1
CH2 (CC2S=10, CC2P=1) → captures FALLING edges → stores in CCR2
Both CH1 and CH2 read the same physical pin PA0 (indirect TI1 mapping)
```

### Measurement Algorithm
```
TIM5->SR = 0;
1. Wait CC1IF → trise1 = CCR1 → clear CC1IF
2. Wait CC2IF → tfall  = CCR2 → clear CC2IF
3. Wait CC1IF → trise2 = CCR1 → clear CC1IF

period_us  = trise2 - trise1
high_us    = tfall  - trise1
freq_hz    = 1,000,000 / period_us
duty_pct   = high_us × 100 / period_us
```

### Circuit Connections
```
NUCLEO-64 PA6  ──[jumper wire]──  NUCLEO-64 PA0
                (TIM3 output)         (TIM5 input)

PA6: D12 on Arduino header
PA0: A0 on Arduino header
No external components needed — internal loopback via breadboard jumper
```

## Shared Driver Reference

### `helper.c` — Functions Used by All Tasks

| Function | Purpose | Key Registers |
|----------|---------|---------------|
| `SystemClock_Config()` | SYSCLK = 180 MHz via PLL | `RCC->PLLCFGR`, `FLASH->ACR`, `PWR->CR` |
| `USART2_Init()` | 115200 8N1 on PA2/PA3 | `USART2->BRR = 0x187`, `CR1` |
| `USART2_SendString()` | Poll TXE then TC | `USART2->SR`, `USART2->DR` |
| `LED_Init()` | PA5 → GP output | `GPIOA->MODER` |
| `LED_On/Off/Toggle()` | Control PA5 | `GPIOA->ODR` |
| `TIM6_Init()` | 1 µs free-running tick | `TIM6->PSC=89`, `ARR=0xFFFF` |
| `delay_us(us)` | µs blocking delay | 16-bit wrap-safe `TIM6->CNT` |
| `delay_ms(ms)` | ms blocking delay | loops `delay_us(1000)` |

## Building & Flashing

### Requirements
- STM32CubeIDE ≥ 1.12
- ST-Link USB driver
- Serial terminal (PuTTY / Tera Term / minicom) at **115200 8N1**

### Steps
```bash
# 1. Open STM32CubeIDE
# 2. Import → Existing Projects into Workspace → select task folder
# 3. Build: Ctrl+B  (or Project → Build All)
# 4. Flash: Run → Debug (F11) or Run (Ctrl+F11)
# 5. Open serial terminal on the ST-Link Virtual COM port
```

### Verify Clock is Correct
```c
// Add this sanity check after SystemClock_Config():
// SystemCoreClockUpdate();
// Expected: SystemCoreClock == 180000000
```

## Common Pitfalls

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| No USART output | USART2 not clocked or PA2 wrong AF | Check `USART2EN`, `AFR[0]` = 7 for PA2 |
| Timer never starts | Forgot `TIM_CR1_CEN` | Add `TIMx->CR1 \|= TIM_CR1_CEN` |
| Wrong frequency | Shadow registers not loaded | Add `TIMx->EGR = TIM_EGR_UG` after PSC/ARR |
| TIM1 output silent | Missing MOE | Add `TIM1->BDTR = TIM_BDTR_MOE` |
| Hard fault on `sinf()` | FPU disabled | `SCB->CPACR \|= (0xF<<20); __DSB(); __ISB()` |
| WS2812B wrong colours | GRB ≠ RGB confusion | Pack as `(G<<16)\|(R<<8)\|B` |
| Servo jitters | Wrong ARR/PSC for 50 Hz | PSC=99, ARR=17999 for 90 MHz clock |
| Input capture hangs | PA6 not jumpered to PA0 | Connect PA6 → PA0 with wire |
| `delay_ms` infinite loop | TIM6 not started before call | Call `TIM6_Init()` first |
| Buzzer no sound | Active buzzer used | Replace with passive piezo buzzer |

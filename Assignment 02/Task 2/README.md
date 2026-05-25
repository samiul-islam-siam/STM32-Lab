# Task 2 — Delay Generation

## Goal

Implement precise blocking delay functions using **TIM6** on the STM32F446RE, including:

1. Microsecond delay (`delay_us`)
2. Millisecond delay (`delay_ms`)
3. Second delay (`delay_s`)
4. Hour-minute-second delay (`delay_hms`)
5. LED blinking demonstration using generated delays

The project demonstrates how a hardware timer can be used for accurate software timing.


## About TIM6

TIM6 is a **basic 16-bit timer** available on STM32 microcontrollers.

It is commonly used for:

- Time-base generation
- Blocking delays
- Trigger generation
- DAC triggering

Unlike advanced timers, TIM6 has no GPIO output channels and is ideal for internal timing applications.

| Parameter | Value |
|------------|-------|
| Timer | TIM6 |
| Counter Width | 16-bit |
| Clock Source | APB1 Timer Clock |
| Usage | Delay generation |
| Mode | Up-counter |



## Timer Configuration

### TIM6 Clock

```text
System Clock = 180 MHz
APB1 Prescaler = 4
TIM6 Clock = 2 × APB1 = 90 MHz
```

### Prescaler Setup

```text
PSC = 89
Timer Tick = 90 MHz / (89 + 1)
           = 1 MHz
           = 1 µs per tick
```

### Auto Reload Register

```text
ARR = 0xFFFF
Maximum delay range = 65535 µs
```

This allows TIM6 to operate as a free-running microsecond counter.


## Delay Functions

### Microsecond Delay

```c
void delay_us(uint16_t us);
```

- Uses TIM6 counter directly
- Resolution: **1 µs**
- Maximum single delay: **65535 µs**
- Overflow-safe using unsigned subtraction


### Millisecond Delay

```c
void delay_ms(uint32_t ms);
```

Implemented using repeated `delay_us(1000)` calls.


### Second Delay

```c
void delay_s(uint32_t sec);
```

Implemented using repeated `delay_ms(1000)` calls.


### Hour-Minute-Second Delay

```c
void delay_hms(uint8_t h, uint8_t m, uint8_t s);
```

Converts time to seconds:

```text
total_seconds = h × 3600 + m × 60 + s
```

Then executes:

```c
delay_s(total_seconds);
```

## Serial Output

<p align="center">
  <img src="/images/task2_output.png" width="40%" alt="UART output">
</p>



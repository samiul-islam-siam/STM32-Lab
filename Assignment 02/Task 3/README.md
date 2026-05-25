# Task 3 — Duration Measurement & Code Profiling

## Goal

Measure and profile execution time of various code blocks on the STM32F446RE using:

1. **TIM2 free-running timer** (microsecond resolution)
2. **ARM Cortex-M4 DWT cycle counter**
3. Interrupt-based millisecond timing using **TIM2 + NVIC**

The project demonstrates low-level performance measurement techniques commonly used in embedded systems for:
- benchmarking
- runtime analysis
- timing verification
- optimisation studies

## About the Profiling Methods

### 1. TIM2 Free-Running Timer

TIM2 is configured as a continuously running 32-bit timer.

```text
TIM2 Clock = 90 MHz
PSC = 89
Timer Tick = 1 MHz
Resolution = 1 µs per tick
```

The timer continuously increments:

```text
0 → 1 → 2 → ... → 0xFFFFFFFF
```

Execution duration is measured by:

```text
elapsed = end_count - start_count
```

Because unsigned subtraction is used, overflow handling is automatic.

### 2. DWT Cycle Counter

The ARM Cortex-M4 core includes a hardware cycle counter:

```text
DWT->CYCCNT
```

It increments every CPU cycle.

At CPU Frequency = 180 MHz, we obtain: 1 cycle ≈ 5.56 ns.

This provides nanosecond-scale profiling precision.

## Profiling Targets

The following code blocks are benchmarked:

#### 1. Bubble Sort (Worst Case)

#### 2. delay_ms(100)

#### 3. USART2_SendString()

#### 4. Integer Square Root Benchmark

#### 5. Memory Copy Benchmark

## DWT Timing Formula

Cycle count conversion:

```text
ns = cycles × 1000 / 180
```

## Profiling Macro

The profiling macro measures DWT cycles and TIM2 elapsed time for the same code block.

```c
#define PROFILE(num, label, block)
```

Workflow:

```text
1. Capture start timestamps
2. Execute code block
3. Capture end timestamps
4. Compute elapsed time
5. Print formatted table
```

## Serial Output

<p align="center">
  <img src="/images/task3_output.png" width="60%" alt="Task 3 UART Output">
</p>


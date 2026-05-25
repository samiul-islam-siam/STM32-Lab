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



# About the Profiling Methods

## 1. TIM2 Free-Running Timer

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



## 2. DWT Cycle Counter

The ARM Cortex-M4 core includes a hardware cycle counter:

```text
DWT->CYCCNT
```

It increments every CPU cycle.

At:

```text
CPU Frequency = 180 MHz
```

we obtain:

```text
1 cycle ≈ 5.56 ns
```

This provides nanosecond-scale profiling precision.




# Profiling Targets

The following code blocks are benchmarked.



# 1. Bubble Sort (Worst Case)

## Configuration

```text
N = 100
Input = Reverse Sorted
```

Worst-case complexity:

```text
O(N²)
```

The array:

```text
100, 99, 98, ..., 1
```

forces maximum swaps and comparisons.



# 2. delay_ms(100)

Profiles blocking software delay execution.

Expected duration:

```text
≈ 100000 µs
```

Used to validate timer accuracy.



# 3. USART2_SendString()

Profiles UART transmission time.

String:

```text
"PROFILING: STM32F446RE USART2 @ 115200 baud OK!"
```

Transmission time estimate:

```text
47 bytes × 10 bits / 115200
≈ 4080 µs
```



# 4. Integer Square Root Benchmark

Uses Newton-Raphson integer square root:

```c
isqrt()
```

Executed:

```text
1000 times
```

Measures arithmetic-heavy computation performance.


# 5. Memory Copy Benchmark

Copies:

```text
512 bytes
```

using byte-by-byte transfer.

Used to measure:
- memory throughput
- loop efficiency
- compiler optimisation effects



# DWT Timing Formula

Cycle count conversion:

```text
ns = cycles × 1000 / 180
```

because:

```text
180 MHz = 180 cycles per µs
```



# Profiling Macro

The profiling macro measures:

- DWT cycles
- TIM2 elapsed time

for the same code block.

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


# Serial Output

<p align="center">
  <img src="/images/task3_output.png" width="75%" alt="Task 3 UART Output">
</p>


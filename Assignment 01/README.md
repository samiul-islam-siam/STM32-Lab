# CSE 2206: Lab-01

## Target Hardware

| Item | Detail |
|---|---|
| Board | NUCLEO-F446RE |
| MCU | STM32F446RET6 (Cortex-M4, up to 180 MHz) |
| Toolchain | ARM GCC + STM32CubeIDE / Makefile |
| CMSIS headers | `stm32f446xx.h` |

## System Clock Configuration (Labs 2A–5A)

All labs from 2A onward use the same PLL setup:

| Parameter | Value |
|---|---|
| Source | HSI (16 MHz) |
| PLLM | 8 |
| PLLN | 180 |
| PLLP | 2 → SYSCLK = **180 MHz** |
| AHB prescaler | /1 → HCLK = 180 MHz |
| APB1 prescaler | /4 → PCLK1 = **45 MHz** |
| APB2 prescaler | /2 → PCLK2 = 90 MHz |
| Flash latency | 5 WS |
| OverDrive | Enabled |

Lab 1A runs on the default **HSI 16 MHz** (no PLL).

## Lab 1A — LED Blink

Blinks the on-board LED (LD2) at 1 Hz using a software delay loop.

### Key Registers

| Register | Setting |
|---|---|
| `RCC->AHB1ENR` | Enable GPIOA clock |
| `GPIOA->MODER[11:10]` | `01` — Output mode for PA5 |
| `GPIOA->OTYPER[5]` | `0` — Push-pull |
| `GPIOA->BSRR` | Bit-set/reset to drive PA5 high/low |

### Circuit

No external components needed. Uses the on-board:

```
MCU PA5 ──[330Ω on-board]── LD2 (green LED) ── GND
```

> LD2 is already wired to PA5 on the Nucleo-64 board.

---

## Lab 2A — UART2 Polling TX/RX

Initializes USART2 at **115200 8N1** on the APB1 bus (45 MHz). Sends a banner string then echoes every received character back to the host — fully blocking (polling).

### Baud Rate Calculation

```
USARTDIV = fCK / (16 × Baud) = 45 000 000 / (16 × 115200) ≈ 24.414
Mantissa = 24  →  BRR[15:4]
Fraction = round(0.414 × 16) = 7  →  BRR[3:0]
BRR = 0x0187
```

### Pin Mapping

| Pin | Function | AF |
|---|---|---|
| PA2 | USART2_TX | AF7 |
| PA3 | USART2_RX | AF7 |

### Connection

The Nucleo-64 has an on-board ST-LINK virtual COM port that internally routes to PA2/PA3:

```
PC (USB)  ←→  ST-LINK VCP  ←→  PA2 (TX) / PA3 (RX)
```

Connect with any serial terminal (PuTTY, minicom, Tera Term) at **115200 8N1**. No external hardware required.

## Lab 3A — External Interrupt on User Button

Configures PC13 (B1 User Button) as a falling-edge EXTI source. Each button press toggles PA5 (LD2) inside the ISR.

### Key Configuration Steps

1. Enable GPIOA, GPIOC, and SYSCFG clocks.
2. PA5 → output; PC13 → input (default, no pull needed — button has external pull-up on Nucleo).
3. Route Port C to EXTI line 13 via `SYSCFG->EXTICR[3]` bits `[7:4] = 0b0010`.
4. Enable falling-edge trigger (`EXTI->FTSR`) and unmask (`EXTI->IMR`).
5. Enable `EXTI15_10_IRQn` in NVIC, priority 1.

### Circuit

No external components needed. Uses on-board:

```
3.3V ──[4.7kΩ pull-up on board]── PC13 ──[B1 button]── GND
```

> The User Button (B1) is already connected to PC13 on the Nucleo-64 board.

## Lab 4A — UART2 Receive Interrupt + Ring Buffer

Receives characters into a 128-byte ring buffer inside the `USART2_IRQHandler`. The main loop polls a `lineReady` flag and echoes the complete line when `\r` is received.

### Ring Buffer

```c
volatile char ring[128];   // circular storage
volatile uint8_t head;     // written by ISR
volatile uint8_t tail;     // consumed by main
volatile uint8_t lineReady;
```

The ISR writes `head`; main reads `tail`. Both modulo `BUF_SIZE` — no locking needed on Cortex-M4 with a single ISR at a fixed priority.

### USART2 ISR Handles

- `RXNE` — new byte available → store to ring buffer, set `lineReady` on `\r`.
- `ORE` — overrun error → cleared by reading `DR`.

### Connection

Same as Lab 2A — ST-LINK VCP on PA2 (TX) / PA3 (RX), 115200 8N1.

## Lab 5A — UART2 DMA TX (Stream 6 Ch4) + DMA RX (Stream 5 Ch4)

Configures DMA1 for both USART2 TX and RX without HAL. On startup, transmits a demo string non-blocking via DMA TX. 
The DMA RX fires an interrupt after every 64 bytes received and echoes the buffer back.

### DMA Channel Assignment (DMA1)

| Direction | Stream | Channel | Trigger |
|---|---|---|---|
| TX (Mem → Periph) | Stream 6 | Ch 4 | USART2_TX |
| RX (Periph → Mem) | Stream 5 | Ch 4 | USART2_RX |

### DMA TX Flow

```
DMA_UART_Transmit(data, len)
    └─ Set M0AR, NDTR
    └─ Enable USART_CR3_DMAT
    └─ Enable DMA1_Stream6 (EN)
         └─ [transfer complete] → DMA1_Stream6_IRQHandler
                └─ Clear TCIF6
                └─ Disable DMAT
                └─ Disable Stream6
```

### DMA RX Flow

```
DMA1_Stream5 always armed (NDTR=64)
    └─ [64 bytes received] → DMA1_Stream5_IRQHandler
             └─ Clear TCIF5
             └─ Echo rxBuf via DMA_UART_Transmit()
             └─ Restart Stream5 (NDTR=64, EN)
```

> **Lab manual note:** The commented-out circular mode (`DMA_SxCR_CIRC`) removes the need to restart the stream in software. 
> This lab uses the non-circular (normal) mode with a TC interrupt and manual restart, showing the mechanics explicitly.

### Register Summary

| Register | Purpose |
|---|---|
| `DMA1_Stream6->PAR` | Peripheral address → `&USART2->DR` |
| `DMA1_Stream6->M0AR` | Source buffer pointer |
| `DMA1_Stream6->NDTR` | Number of bytes to transfer |
| `DMA1_Stream6->CR` | Channel sel, direction, MINC, TCIE |
| `DMA1_Stream5->M0AR` | Destination → `rxBuf[64]` |
| `USART2->CR3` | `DMAT` / `DMAR` enable bits |

### Connection

Same as Labs 2A and 4A — ST-LINK VCP on PA2 (TX) / PA3 (RX), 115200 8N1. No external hardware required.

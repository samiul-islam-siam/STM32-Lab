# CSE 2206: Lab-01

## System Clock Configuration

| Parameter      | Value                    |
|----------------|--------------------------|
| Source         | HSI (16 MHz)             |
| PLLM           | 8                        |
| PLLN           | 180                      |
| PLLP           | 2 → SYSCLK = **180 MHz** |
| AHB prescaler  | /1 → HCLK = 180 MHz      |
| APB1 prescaler | /4 → PCLK1 = **45 MHz**  |
| APB2 prescaler | /2 → PCLK2 = 90 MHz      |
| Flash latency  | 5 WS                     |
| OverDrive      | Enabled                  |

Default clock is **HSI 16 MHz** (no PLL).

## Task 1 — LED Blink

Blinks the on-board LED (LD2) at 1 Hz using a software delay loop.

## Task 2 — UART2 Polling

Initializes USART2 at **115200 8N1** on the APB1 bus (45 MHz). Sends a banner string then echoes every received character back to the host — fully blocking (polling).

### Baud Rate Calculation

```
USARTDIV = fCK / (16 × Baud) = 45 000 000 / (16 × 115200) ≈ 24.414
Mantissa = 24  →  BRR[15:4]
Fraction = round(0.414 × 16) = 7  →  BRR[3:0]
BRR = 0x0187
```

### Pin Mapping

| Pin   | Function    | AF    |
|-------|-------------|-------|
| PA2   | USART2_TX   | AF7   |
| PA3   | USART2_RX   | AF7   |

### Connection

The Nucleo-64 has an on-board ST-LINK virtual COM port that internally routes to PA2/PA3:

```
PC (USB)  ←→  ST-LINK VCP  ←→  PA2 (TX) / PA3 (RX)
```

Connect with any serial terminal at **115200 8N1**. No external hardware required.

## Task 3 — External Interrupt on User Button

Configures PC13 (B1 User Button) as a falling-edge EXTI source. Each button press toggles PA5 (LD2) inside the ISR.

## Task 4 — UART2 Receive Interrupt + Ring Buffer

Receives characters into a 128-byte ring buffer inside the `USART2_IRQHandler`. The main loop polls a `lineReady` flag and echoes the complete line when `\r` is received.

### Ring Buffer

```c
volatile char ring[128];   // circular storage
volatile uint8_t head;     // written by ISR
volatile uint8_t tail;     // consumed by main
volatile uint8_t lineReady;
```

The ISR writes `head`; main reads `tail`. Both modulo `BUF_SIZE` — no locking needed on Cortex-M4 with a single ISR at a fixed priority.

## Task 5 — UART2 DMA TX + DMA RX

Configures DMA1 for both USART2 TX and RX. On startup, transmits a demo string non-blocking via DMA TX. 
The DMA RX fires an interrupt after every 64 bytes received and echoes the buffer back.

### DMA Channel Assignment (DMA1)

| Direction           | Stream     | Channel | Trigger     |
|---------------------|------------|---------|-------------|
| TX (Mem → Periph)   | Stream 6   | Ch 4    | USART2_TX   |
| RX (Periph → Mem)   | Stream 5   | Ch 4    | USART2_RX   |
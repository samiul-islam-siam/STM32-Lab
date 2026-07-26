# CSE2206 Lab 04: ADC Multi-Resolution Test & Flash Storage

## Description

Firmware that stores student identity data once into flash, samples an
analog input at four ADC resolutions (12/10/8/6-bit) on command, converts the
readings to voltage, and saves the results into flash so they persist across
power cycles. All output is printed over a UART serial terminal.

## Circuit Connections

| Signal            | MCU Pin   | Connected To                     |
|-------------------|-----------|----------------------------------|
| ADC1_IN0          | PA0       | Analog input signal (0–3.3V)     |
| USART2_TX         | PA2       | ST-Link Virtual COM Port (RX)    |
| USART2_RX         | PA3       | ST-Link Virtual COM Port (TX)    |
| SWDIO / SWCLK     | PA13/PA14 | ST-Link (on-board debugger)      |
| GND               | GND       | Common ground with signal source |

## CubeMX Config

**Pinout & Configuration**

System Core:
```
SYS: Debug: Serial Wire
RCC: HSI (internal), PLL ON
```

Analog:
```
ADC1: IN0 (PA0)
Parameter Settings:
  Resolution: 12-bit
  Data Alignment: Right
  Scan Conversion Mode: Disabled
  Continuous Conversion Mode: Disabled
  Number Of Conversion: 1
  External Trigger: Software
  Sampling Time: 3 Cycles
```

Connectivity:
```
USART2: Mode: Asynchronous
  Baud Rate: 115200
  Word Length: 8 Bits
  Parity: None
  Stop Bits: 1
```

**Clock Configuration**
```
Put HCLK = 180 --> It will config automatically
```

## Properties Config

**Warning:**
> ⚠️ The float formatting support is not enabled, check your MCU Settings from
"Project Properties > C/C++ Build > Settings > Tool Settings", or add
manually "-u _printf_float" in linker flags.

**Solution:**
> 💡 Go to Project > Properties > C/C++ Build > Settings > Tool Settings >
MCU/MPU Settings > Check "Use float with printf from newlib-nano
(-u _printf_float)" and Check "Use float with scanf from newlib-nano
(-u _scanf_float)" > Apply and Close

> 💡 Go to Project > Properties > C/C++ Build > Settings > Tool Settings >
MCU/MPU GCC Linker > Miscellaneous > Other flags > Add... > "-lm" > Apply and
Close

## Debug Config

**Debugger > Reset Settings > Reset: None** must be selected.

1. Run > Debug Configurations…
2. Select the project's configuration under STM32 Cortex-M C/C++ Application
3. Debugger tab > Reset Behavior / Reset Settings
4. Set Reset to **None**
5. Apply > Close

> ⚠️ Without this, debug/flash sessions force an MCU reset that can interrupt a
flash erase/write in progress, corrupting the stored identity or results
data.

## Cautions

- Identity provisioning runs once — reflash with it commented out afterward.
- Any flash write erases the whole sector first; don't interleave with other
  data in that sector.
- Never power-cycle or reset mid flash-write.
- Keep the analog input within 0–3.3V.
- Match terminal settings exactly: 115200 8N1, no flow control.

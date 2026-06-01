# LCD 1602A Bare-Metal Driver
LCD 1602A i.e. **16x2 display** Bare Metal driver with example code for STM32F446RE. Scroll down for wiring diagram.

## Circuit Connections

### LCD 1602A (Display)

| LCD Pin | LCD Signal | STM32 Pin                  |
|---------|------------|----------------------------|
| 1       | VSS        | GND                        |
| 2       | VDD        | 5V                         |
| 3       | VO         | Potentiometer middle pin   |
| 4       | RS         | PB5 (D4)                   |
| 5       | RW         | GND                        |
| 6       | EN         | PB4 (D5)                   |
| 7–10    | D0–D3      | Not connected (4-bit mode) |
| 11      | D4         | PC7 (D9)                   |
| 12      | D5         | PB6 (D10)                  |
| 13      | D6         | PA7 (D11)                  |
| 14      | D7         | PA6 (D12)                  |
| 15      | A (BL+)    | 3.3V                       |
| 16      | K (BL−)    | GND                        |

### Potentiometer (Contrast Control)

| Pot Pin | Connect To     |
|---------|----------------|
| Left    | GND            |
| Middle  | LCD pin 3 (VO) |
| Right   | 3.3V           |

> Connect VO to GND if potentiometer is not available. You can also add resistors instead of potentiometer.

### Wiring Diagram

<p style="text-align: center;">
  <img src="/images/wiring_direct.jpg" width="48%" alt="wiring without pot">
  <img src="/images/wiring_pot.jpg" width="48%" alt="wiring with pot">
</p>
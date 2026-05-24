# STM32 Dino Game (HAL) 🦕
A Chrome Dino-style game running on a 16x2 LCD, built with STM32F446RE Nucleo board using STM32CubeIDE.

## How to Play

- Press the button to jump over cacti
- If a bird appears, stay on the ground and let it fly over
- Game ends on collision — reset button to play again
- Score is shown on the top right, speed increases over time

## Hardware

- STM32F446RE Nucleo-64 board
- LCD 1602A (HD44780 compatible, 16x2)
- Passive buzzer (2 pin)
- Push button (4 pin)
- 10kΩ potentiometer (LCD contrast)
- 220Ω resistor (LCD backlight)
- Breadboard and jumper wires

## Circuit Connections

### LCD 1602A

| LCD Pin | LCD Signal | STM32 Pin                  |
|---------|------------|----------------------------|
| 1       | VSS        | GND                        |
| 2       | VDD        | 3.3V                       |
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

> Connect VO to GND if potentiometer is not available. You can also add resistors instead of potentiometer.

### Push Button

| Button Pin | Connect To |
|------------|------------|
| One side   | PA0 (A0)   |
| Other side | 3.3V       |

> PA0 is configured as input with internal pull-down. Button reads HIGH when pressed.

### Passive Buzzer

| Buzzer Pin | Connect To |
|------------|------------|
| Positive   | PB0 (A3)   |
| Negative   | GND        |

### Potentiometer (Contrast)

| Pot Pin | Connect To     |
|---------|----------------|
| Left    | GND            |
| Middle  | LCD pin 3 (VO) |
| Right   | 3.3V           |

## Build & Flash

1. Open project in STM32CubeIDE
2. Build with **Project → Build All**
3. Flash with **Run → Debug** via onboard ST-Link
4. Adjust potentiometer until characters are clearly visible

## Speed Tuning

Edit these three variables in `main.c` to adjust difficulty:

| Variable        | Default | Effect                                            |
|-----------------|---------|---------------------------------------------------|
| `period2`       | 100     | Starting speed — lower is faster                  |
| `acceleration`  | 1       | Speed increase per cycle — higher ramps up faster |
| Min cap in loop | 30      | Fastest possible speed — lower is harder          |

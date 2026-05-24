# Morse Code Decoder using STM32

A simple Morse code decoder built with STM32F446RE.  
Short and long button presses are converted into Morse code symbols and decoded characters are printed through UART.

## Hardware Used

- STM32F446RE Development Board
- Push Button
- LED
- 220Ω Resistor
- Passive Buzzer
- USB to UART Adapter
- Breadboard & Jumper Wires

## Pin Configuration

| Peripheral   | STM32 Pin |
|--------------|-----------|
| Push Button  | PC13      |
| LED          | PA6       |
| Buzzer       | PB1       |
| UART2 TX     | PA2       |

## Circuit Connections

| Signal      | MCU Pin | Connection                       |
|-------------|---------|----------------------------------|
| Button      | PC13    | One leg → PC13, other leg → GND  |
| LED         | PA6     | PA6 → 220Ω resistor → LED anode  |
| LED Cathode | GND     | LED cathode → GND                |
| Buzzer      | PB1     | PB1 → Buzzer +                   |
| Buzzer GND  | GND     | Buzzer − → GND                   |
| UART TX     | PA2     | PA2 → RX of USB-UART Adapter     |

## How It Works

- Press the button briefly (`< 400 ms`) for a DOT (`.`)
- Press the button longer (`>= 400 ms`) for a DASH (`-`)
- After `1200 ms` of no input, the Morse sequence is decoded automatically
- The decoded character is printed on the UART terminal

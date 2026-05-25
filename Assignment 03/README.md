# CSE 2206: Lab-03

STM32F446RE with the BME280 environmental sensor using two different communication protocols: SPI (Part A) and I2C (Part B). 
Both parts produce identical sensor output; the difference lies entirely in the bus protocol and its register-level configuration.

<p style="text-align: center;">
  <img src="/images/GY-BMEP280_front_back.jpg" width="25%" alt="front and back">
  <img src="/images/GY-BMEP280_with_header.jpg" width="25%" alt="with header">
  <img src="/images/GY-BMEP280_dimension.jpg" width="25%" alt="dimensions">
</p>

<br>

Warning:
> ⚠️ The float formatting support is not enabled, check your MCU Settings from "Project Properties > C/C++ Build > Settings > Tool Settings", or add manually "-u _printf_float" in linker flags.

Solution:
> 💡 Go to Project > Properties > C/C++ Build > Settings > Tool Settings > MCU/MPU Settings >
      Check Use float with printf from newlib-nano (-u _printf_float) and 
      Check Use float with scanf from newlib-nano (-u _scanf_float) > Apply and Close

## Circuit Diagram:
<p style="text-align: center;">
   <img src="/images/GY-BMP280-module-circuit.png" width="50%" alt="NUCLEO-F446RE board front layout">
</p>

## SPI communication:
| GY-BMP280 | STM32F446RE  |
| ------ |--------------|
| VCC    | 3.3V         |
| GND    | GND          |
| SCL    | PB10 (D6)    |
| SDA    | PC1 (A4)     |
| CSB    | PB9 (D14)    |
| SDO    | PC2 (CN7-35) |

## I2C communication:
| GY-BMP280 | STM32F446RE  |
| --------- |--------------|
| VCC       | 3.3V         |
| GND       | GND          |
| SCL       | PB6 (D10)    |
| SDA       | PB7 (CN7-21) |
| CSB       | 3.3V         |
| SDO       | GND          |


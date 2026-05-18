# CSE 2206: Lab-03

STM32F446RE with the BME280 environmental sensor using two different communication protocols: SPI (Part A) and I2C (Part B). 
Both parts produce identical sensor output; the difference lies entirely in the bus protocol and its register-level configuration.

<br>

Warning:
> ⚠️ The float formatting support is not enabled, check your MCU Settings from "Project Properties > C/C++ Build > Settings > Tool Settings", or add manually "-u _printf_float" in linker flags.

Solution:
> 💡 Go to Project > Properties > C/C++ Build > Settings > Tool Settings > MCU/MPU Settings >
      Check Use float with printf from newlib-nano (-u _printf_float) and 
      Check Use float with scanf from newlib-nano (-u _scanf_float) > Apply and Close
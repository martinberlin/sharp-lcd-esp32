## Sharp LCD PCB

This is a small sized PCB to control this display with a ESP32-C3.
It has a DS3231 RTC chip with it's SQW/INT pin wired directly to the EXTCOMIN pin of the LCD.

> The display requires that the polarity across the Liquid Crystal Cell is reversed at a constant frequency. This polarity inversion prevents charge building up within the cell

![sharp PCB front](https://raw.githubusercontent.com/martinberlin/H-spi-adapters/master/1.54-sharp-lcd/schematic/sharp-lcd-front.png)

![sharp PCB back](https://raw.githubusercontent.com/martinberlin/H-spi-adapters/master/1.54-sharp-lcd/schematic/sharp-lcd-back.png)

[Sharp PCB controller](https://github.com/martinberlin/H-spi-adapters/tree/master/1.54-sharp-lcd) is open source. Find the KiCad files in the repository link.

## LCD demos with ESP32C3

To be described after the first demos are coded.

[Short video preview](https://twitter.com/martinfasani/status/1664759302995734530)

#include "driver/gpio.h"

// PCB Pins for LCD SPI
#define SHARP_SCK  6
#define SHARP_MOSI 0
#define SHARP_SS   7

// External COM Inversion Signal
#define LCD_EXTMODE GPIO_NUM_3
#define LCD_DISP    GPIO_NUM_2

// Used either fro Touch INT or RTC int (Needs to get additional wiring)
#define GPIO_RTC_INT GPIO_NUM_8

// LCD 1-bit color
#define BLACK 0
#define WHITE 1


#include "driver/gpio.h"

// PCB Pins for LCD SPI
#define SHARP_SCK  1
#define SHARP_MOSI 2
#define SHARP_SS   3

// External COM Inversion Signal
#define LCD_EXTMODE GPIO_NUM_14
#define LCD_DISP    GPIO_NUM_4

// Buttons
#define PCB_BUTTON_1 GPIO_NUM_8
#define PCB_BUTTON_2 GPIO_NUM_4
#define PCB_BUTTON_3 GPIO_NUM_10
#define BUTTON_1 int(PCB_BUTTON_1)
#define BUTTON_2 int(PCB_BUTTON_2)
#define BUTTON_3 int(PCB_BUTTON_3)
#define GPIO_INPUT_PIN_SEL  ((1ULL<<PCB_BUTTON_1) | (1ULL<<PCB_BUTTON_2) | (1ULL<<PCB_BUTTON_3))

// LCD 1-bit color
#define BLACK 0
#define WHITE 1

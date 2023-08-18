
// ATENTION Only LCD test. No 1 Hz SQW provided. Do not run it for long!!!
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Adafruit_SharpMem.h>
#include "driver/gpio.h"

#include "ds3231.h"
#include "FT6X36.h"

// TOUCH: INTGPIO is touch interrupt, goes low when it detects a touch, which coordinates are read by I2C
FT6X36 ts(CONFIG_TOUCH_INT);
// PCB Pins for LCD SPI
// C3
#define SHARP_SCK  6
#define SHARP_MOSI 0
#define SHARP_SS   7

//ESP32
/* #define SHARP_SCK  18
#define SHARP_MOSI 23
#define SHARP_SS   5 */

#define BLACK 0
#define WHITE 1

// External COM Inversion Signal
#define LCD_EXTMODE GPIO_NUM_3
//#define LCD_DISP    GPIO_NUM_2 //C3
#define LCD_DISP    GPIO_NUM_4

int hSize; // 1/2 of lesser of display width or height

// Set the size of the display here, e.g. WIDTHxHEIGHT
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 400, 240);

extern "C"
{
   void app_main();
}


void delay(uint32_t millis) { vTaskDelay(millis / portTICK_PERIOD_MS); }

void uxDraw() {
  display.drawRoundRect(0, 0, 40, 40, 4, 0);
}

uint16_t t_counter = 0;

std::string eventToString(TEvent e) {
  std::string eventName = "";
    switch (e)
    {
    case TEvent::Tap:
      eventName = "tap";
      break;
    case TEvent::DragStart:
      eventName = "DragStart";
      break;
    case TEvent::DragMove:
      eventName = "DragMove";
      break;
    case TEvent::DragEnd:
      eventName = "DragEnd";
      break;
    default:
      eventName = "UNKNOWN";
      break;
    }
    return eventName;
}


void touchEvent(TPoint p, TEvent e)
{
  #if defined(DEBUG_COUNT_TOUCH) && DEBUG_COUNT_TOUCH==1
    ++t_counter;
    ets_printf("e %x %d  ",e,t_counter); // Working
  #endif
  // Activate this to mark only TAP events
  //if (e != TEvent::Tap && e != TEvent::DragStart && e != TEvent::DragMove && e != TEvent::DragEnd)
  //  return;
  
  // Convert 2.7" touch resolution 264*176 into 400*240 LCD
  uint16_t lcd_x = p.x *1.37; // Max 240
  uint16_t lcd_y = p.y *1.51; // Max 400
  if (lcd_x<50 && lcd_y<50) {
   display.clearDisplay();
   uxDraw();
   display.refresh();
   return;
  }
  display.fillCircle(lcd_x, lcd_y, 2, 0);
  display.refresh();
  printf("X:%d Y:%d LCDx:%d y:%d E:%s\n", p.x, p.y, lcd_x, lcd_y, eventToString(e).c_str());
}

void app_main() {
  printf("Hello LCD!\n\n");
  ts.begin(FT6X36_DEFAULT_THRESHOLD, display.width(), display.height());
  ts.setRotation(0);
  ts.registerTouchHandler(touchEvent);

  //gpio_set_direction(GPIO_NUM_7, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_EXTMODE, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_DISP, GPIO_MODE_OUTPUT);// Display On(High)/Off(Low) 
  gpio_set_level(LCD_DISP, 1);delay(50);

  gpio_set_level(LCD_EXTMODE, 1); // Using Ext com in HIGH mode-> Signal sent by RTC at 1 Hz

  display.begin();
  display.clearDisplay();
  display.setRotation(1);
  uxDraw();
  
  for (;;) {
      ts.loop();
    }
}
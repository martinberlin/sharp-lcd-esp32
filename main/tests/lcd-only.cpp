
// ATENTION Only LCD test. No 1 Hz SQW provided. Do not run it for long!!!
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Adafruit_SharpMem.h>
#include "driver/gpio.h"

#include "ds3231.h"

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

void testdrawline() {
  for (int i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, BLACK);
    display.refresh();
  }
  for (int i=0; i<display.height(); i+=4) {
    display.drawLine(0, 0, display.width()-1, i, BLACK);
    display.refresh();
  }
  delay(250);

  display.clearDisplay();
  for (int i=0; i<display.width(); i+=4) {
    display.drawLine(0, display.height()-1, i, 0, BLACK);
    display.refresh();
  }
  for (int i=display.height()-1; i>=0; i-=4) {
    display.drawLine(0, display.height()-1, display.width()-1, i, BLACK);
    display.refresh();
  }
  delay(250);

  display.clearDisplay();
  for (int i=display.width()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, i, 0, BLACK);
    display.refresh();
  }
  for (int i=display.height()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, 0, i, BLACK);
    display.refresh();
  }
  delay(250);

  display.clearDisplay();
  for (int i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, BLACK);
    display.refresh();
  }
  for (int i=0; i<display.width(); i+=4) {
    display.drawLine(display.width()-1, 0, i, display.height()-1, BLACK);
    display.refresh();
  }
  delay(250);
}


void app_main() {
  printf("Hello LCD!\n\n");
  //gpio_set_direction(GPIO_NUM_7, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_EXTMODE, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_DISP, GPIO_MODE_OUTPUT);// Display On(High)/Off(Low) 
  gpio_set_level(LCD_DISP, 1);delay(50);

  gpio_set_level(LCD_EXTMODE, 1); // Using Ext com in HIGH mode-> Signal sent by RTC at 1 Hz


  

  display.begin();
  display.clearDisplay();
  display.setRotation(0);
  // Several shapes are drawn centered on the screen.  Calculate 1/2 of
  // lesser of display width or height, this is used repeatedly later.
  printf("Display W:%d H:%d\n\n", display.width(), display.height());

  hSize = display.width() / 2;
  /* printf("Draw filled screen\n");
  display.fillScreen(BLACK);
  display.refresh();
   */
  printf("Draw filled Circle!\n\n\n");
  
  display.fillCircle(hSize,hSize,50, BLACK);
  //display.fillRect(1,1,128,128, BLACK);
  display.refresh();
  //return;
  delay(3000);
  display.clearDisplay();

  display.setTextSize(3);
  display.setTextColor(BLACK);
  display.setCursor(4,10);
  display.cp437(true);
  printf("LCD width %d, hei %d\n",display.width(), display.height());
  display.print("Hola UGO\n\n");
  display.refresh();
  delay(299);
  display.print("GUAPO!!!!");
  display.refresh();
  delay(1000);


printf("draw many lines\n\n\n");
  testdrawline();
  delay(500);
  display.clearDisplay(); 
}
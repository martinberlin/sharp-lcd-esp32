
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Adafruit_SharpMem.h>
#include "driver/gpio.h"

#include "ds3231.h"

// PCB Pins for LCD SPI

//ESP32
#define SHARP_SCK  1
#define SHARP_MOSI 2
#define SHARP_SS   3

// I2C
//#define SDA_GPIO GPIO_NUM_5  //C3
#define SDA_GPIO GPIO_NUM_7  //ESP32
#define SCL_GPIO GPIO_NUM_8
i2c_dev_t dev;

#define BLACK 0
#define WHITE 1

// External COM Inversion Signal
#define LCD_EXTMODE GPIO_NUM_14
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

TaskHandle_t sqw_task;

void sqw_clear(void* pvParameters) {
  for (;;)
  {
      ds3231_clear_alarm_flags(&dev, DS3231_ALARM_1);
      delay(1000);
      //printf("sqw_c\n");
  }
}

void app_main() {
  printf("Hello LCD!\n\n");
  //gpio_set_direction(GPIO_NUM_7, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_EXTMODE, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_DISP, GPIO_MODE_OUTPUT);// Display On(High)/Off(Low) 
  gpio_set_level(LCD_DISP, 1);delay(50);

  gpio_set_level(LCD_EXTMODE, 1); // Using Ext com in HIGH mode-> Signal sent by RTC at 1 Hz


  // Initialize RTC  
  if (true)  {
  if (ds3231_init_desc(&dev, I2C_NUM_0, SDA_GPIO, SCL_GPIO) != ESP_OK) {
      ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
      while (1) { vTaskDelay(1); }
  }

  // Enable SQW for LCD:
  ds3231_enable_sqw(&dev, DS3231_1HZ);

  // No need to clear alarm flags in SQW mode
  //xTaskCreatePinnedToCore(
  //  sqw_clear, /* Task function. */
  //  "sqw_task",    /* name of task. */
  //  10000,     /* Stack size of task */
  //  NULL,      /* parameter of the task */
  //  1,         /* priority of the task */
  //  &sqw_task, /* Task handle to keep track of created task */
  //  0);    /* pin task to core 0 */
  // }


  display.begin();
  display.clearDisplay();
  display.setRotation(1);
  // Several shapes are drawn centered on the screen.  Calculate 1/2 of
  // lesser of display width or height, this is used repeatedly later.
  hSize = display.width() / 2;
  /* printf("Draw filled screen\n");
  display.fillScreen(BLACK);
  display.refresh();
   */
delay(5000);
  printf("Draw filled Circle!\n\n\n");
  
  display.fillCircle(hSize,hSize,50, BLACK);
  //display.fillRect(1,1,128,128, BLACK);
  display.refresh();
  //return;
  delay(8000);
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(4,10);
  display.cp437(true);
  printf("LCD width %d, hei %d\n",display.width(), display.height());
  display.print("Hello UGO\n\n");
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
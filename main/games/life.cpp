/**
 * @file life.cpp
 * @author  delhoume     (github)
 * @adapted martinberlin (github)
 * @source https://github.com/delhoume/ssd1306_adafruit_game_of_life
 * @version 0.1
 * @date 2023-06-29
 */

#include "settings.h"
#include <Adafruit_SharpMem.h>
#include <esp_timer.h>
#include <time.h>
#include <bb_rtc.h>
#include <math.h>  // roundf
const int width = 200;
const int height = 240;
BBRTC rtc;
// These are the GPIO pins on the M5Stack M5StickC PLUS2
// The address and type of RTC will be auto-detected
#define SDA_PIN 7
#define SCL_PIN 8
// Declare ASCII names for each of the supported RTC types
const char *szType[] = {"Unknown", "PCF8563", "DS3231", "RV3032", "PCF85063A"};

struct tm rtcinfo;
// SHARP LCD Class
// Set the size of the display here, e.g. 128x128
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 400, height);

#define USE_SCD40


void delay(uint32_t millis) { vTaskDelay(pdMS_TO_TICKS(millis)); }

#ifdef USE_SCD40
  // SCD4x
  #include "scd4x_i2c.h"
  #include "sensirion_common.h"
  #include "sensirion_i2c_hal.h"

  void scd_read()
{
    int16_t error = 0;
    // int16_t sensirion_i2c_hal_init(int gpio_sda, int gpio_scl);
    sensirion_i2c_hal_init(SDA_PIN, SCL_PIN);

    // Clean up potential SCD40 states
    scd4x_wake_up();
    scd4x_stop_periodic_measurement();
    scd4x_reinit();

    uint16_t serial_0;
    uint16_t serial_1;
    uint16_t serial_2;
    error = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
    if (error)
    {
        printf("Error executing scd4x_get_serial_number(): %i\n", error);
    }
    else
    {
        ESP_LOGI("SCD40", "serial: 0x%04x%04x%04x\n", serial_0, serial_1, serial_2);
    }

    // Start Measurement
    error = scd4x_start_periodic_measurement();
    if (error)
    {
        ESP_LOGE("SCD40", "Error executing scd4x_start_periodic_measurement(): %i\n", error);
        return;
    }

    printf("Waiting for first measurement... (5 sec)\n");
    bool data_ready_flag = false;
    for (uint8_t c = 0; c < 100; ++c)
    {
        // Read Measurement
        sensirion_i2c_hal_sleep_usec(100000);
        // bool data_ready_flag = false;
        error = scd4x_get_data_ready_flag(&data_ready_flag);
        if (error)
        {
            ESP_LOGE("SCD40", "Error executing scd4x_get_data_ready_flag(): %i\n", error);
            continue;
        }
        if (data_ready_flag)
        {
            break;
        }
    }
    if (!data_ready_flag)
    {
        ESP_LOGE("SCD40", "Ready flag is not coming in time");
    }

    uint16_t co2;
    int32_t temperature;
    int32_t humidity;
    error = scd4x_read_measurement(&co2, &temperature, &humidity);
    if (error)
    {
        ESP_LOGE("SCD40", "Error executing scd4x_read_measurement(): %i\n", error);
    }
    else if (co2 == 0)
    {
        ESP_LOGI("SCD40", "Invalid sample detected, skipping.\n");
    }
    else
    {
        scd4x_stop_periodic_measurement();
        float tem = (float)temperature / 1000;
        tem = roundf(tem * 10) / 10;
        float hum = (float)humidity / 1000;
        hum = roundf(hum * 10) / 10;
        ESP_LOGI("SCD40", "CO2 : %u", co2);
        ESP_LOGI("SCD40", "Temp: %d m°C %.1f 0xFC", (int)temperature, tem);
        ESP_LOGI("SCD40", "Humi: %d mRH %.1f %%\n", (int)humidity, hum);

        int cursor_x = 210;
        int cursor_y = 75;
        
        char textbuffer[12];
        snprintf(textbuffer, sizeof(textbuffer), "%d CO2", co2);
        display.setCursor(cursor_x, cursor_y);
        display.print(textbuffer); //CO2
        
        char tempbuffer[12];
        cursor_x = 210;
        cursor_y += 40;
        snprintf(tempbuffer, sizeof(tempbuffer), "%.1f C", tem);
        display.setCursor(cursor_x, cursor_y);
        display.print(tempbuffer);

        char humbuffer[12];
        cursor_x = 210;
        cursor_y += 40;
        snprintf(humbuffer, sizeof(humbuffer), "%.1f %% hum", hum);
        display.setCursor(cursor_x, cursor_y);
        display.print(humbuffer);
        cursor_y += 50;
        display.setCursor(cursor_x, cursor_y);
        display.print("FASANI");
        cursor_y += 30;
        display.setCursor(cursor_x, cursor_y);
        display.print("CORP.");
    }
    ESP_LOGI("SCD40", "power_down()");
    display.refresh();
    delay(1000);
    scd4x_power_down();
    sensirion_i2c_hal_free();
  }
#endif

// FONTS
#include <Ubuntu_M8pt8b.h>

extern "C"
{
    void app_main();
}

uint16_t generateRandom(uint16_t max) {
    if (max>0) {
        srand(esp_timer_get_time());
        return rand() % max;
    }
    return 0;
}
//Current grid, max is 64x32 for 2048 byte Uno...
// width must be multiple of 8
// 8.6 iterations / second
const boolean displayIterations = true;
const uint16_t maxGenerations = 500;

uint8_t grid[width * height];
// should be possible to do without full copy buffer, but how ?
uint8_t newgrid[width * height];

uint8_t GFXsetBit[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
uint8_t GFXclrBit[] = { 0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE };

inline void setPixel(uint8_t* ptr, uint8_t x, uint8_t y, uint8_t color) {
  uint16_t idx = (x + y * width)  / 8;  
  if (color == 1) {
    //     ptr[idx] |= (0x80 >> (x & 7));
    // >> is slow on AVR (Adafruit)
    ptr[idx] |= GFXsetBit[x & 7];
  } else {
    ptr[idx] &= GFXclrBit[x & 7];
  }
  display.drawPixel(x, y, color);
}

inline uint8_t getPixel(uint8_t* ptr, uint8_t x, uint8_t y) {
  return display.getPixel(x, y);
}

//Initialize Grid
void initGrid() {
    for (uint8_t y = 0; y < height; y++) {
       for (uint8_t x = 0; x < width; x++) {
        if (x==0 || x==(width - 1) || y==0 || y== (height - 1)){
          setPixel(grid, x, y, 0);
        } else {
            setPixel(grid, x, y, generateRandom(2)); 
        }
    }   
  }
}

// order makes faster code ?
inline uint8_t getNumberOfNeighbors(uint8_t x, uint8_t y){
    return getPixel(grid, x-1, y)+
  getPixel(grid, x+1, y)+
  getPixel(grid, x-1, y-1)+
  getPixel(grid, x, y-1)+
  getPixel(grid, x+1, y-1)+
  getPixel(grid, x-1, y+1)+
  getPixel(grid, x, y+1)+
  getPixel(grid, x+1, y+1);
}

void computeNewGeneration(){
    for (uint8_t y = 1; y < (height - 1); y++) {
      for (uint8_t x = 1; x < (width - 1); x++) {
        uint8_t neighbors = getNumberOfNeighbors(x, y);
        uint8_t current = getPixel(grid, x, y);
        if (current == 1 && (neighbors == 2 || neighbors == 3 )) {
          setPixel(newgrid, x, y, 1);
        } else if (current==1)  
          setPixel(newgrid, x, y, 0);
        if (current==0 && (neighbors==3)) {
          setPixel(newgrid, x, y, 1);
        } else if (current==0) 
          setPixel(newgrid, x, y, 0);
    }  
  }
  // copy newgrid into grid
  memcpy(grid, newgrid, width * height / 8);
}


void loop() {       
    //Displaying a simple splash screen    
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20,25);
  display.print("Tu padre");
  display.setCursor(20,55);
  display.print("te quiere!");

  display.setCursor(20,95);
  display.print("Buen dia");
  display.setCursor(20,120);
  display.print("NEL");
  display.refresh();
  delay(1500);
  scd_read();

  //display.clearDisplay();
    initGrid();
  for (uint16_t gen = 0; gen < maxGenerations; gen++) {
     computeNewGeneration();
     delay(10);
     display.refresh();
  }

  display.setCursor(20,55);
  display.print("THE END");
  display.refresh();
  delay(2500);
  
}

void app_main()   {
  gpio_set_direction(LCD_EXTMODE, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_DISP, GPIO_MODE_OUTPUT);// Display On(High)/Off(Low) 
  gpio_set_level(LCD_DISP, 1);
  gpio_set_level(LCD_EXTMODE, 1); // Using Ext com in HIGH mode-> Signal sent by RTC at 1 Hz
  delay(20);
  
// Initialize RTC
  int rc = rtc.init(SDA_PIN, SCL_PIN);
    if (rc != RTC_SUCCESS) {
        printf("Error initializing the RTC; stopping...\n");
        while (1) {
            vTaskDelay(1);
        }
    }
  printf("RTC detected and starting SQW\n");
  rtc.setFreq(1000); // Squarewave |_|‾| for display DS3231_1HZ
  //ds3231_enable_sqw(&dev, DS3231_4096HZ); 
    display.begin();
  display.setRotation(0);
  display.clearDisplayBuffer();

  display.setTextColor(BLACK);
  display.setFont(&Ubuntu_M8pt8b);

  while (1) { loop(); }
}


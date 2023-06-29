#include "settings.h"
#include <Adafruit_SharpMem.h>
#include <esp_timer.h>
#include "ds3231.h"
struct tm rtcinfo;
// SHARP LCD Class
// Set the size of the display here, e.g. 128x128
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 128, 128);

// FONTS
#include <Ubuntu_M8pt8b.h>

esp_err_t ds3231_initialization_status = ESP_OK;
i2c_dev_t dev;
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

void delay(uint32_t millis) { vTaskDelay(pdMS_TO_TICKS(millis)); }
//Current grid, max is 64x32 for 2048 byte Uno...
// width must be multiple of 8
// 8.6 iterations / second
const uint8_t width = 128;
const uint8_t height = 128;
const uint8_t pixelSize = 2;
const boolean displayIterations = true;
const uint16_t maxGenerations = 1000;

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
  display.setCursor(20,5);
  display.print("Arduino");
  display.setCursor(20,25);
  display.print("Game of");        
  display.setCursor(20,45);
  display.print("Life");
  display.refresh();
  delay(1500);

  display.clearDisplay();
    initGrid();
//  int t = millis();
  for (uint16_t gen = 0; gen < maxGenerations; gen++) {
     computeNewGeneration();
     delay(10);
     display.refresh();
  } 
  
}

void app_main()   {
  gpio_set_direction(LCD_EXTMODE, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_DISP, GPIO_MODE_OUTPUT);// Display On(High)/Off(Low) 
  gpio_set_level(LCD_DISP, 1);
  gpio_set_level(LCD_EXTMODE, 1); // Using Ext com in HIGH mode-> Signal sent by RTC at 1 Hz
  delay(200);
    // Initialize RTC
ds3231_initialization_status = ds3231_init_desc(&dev, I2C_NUM_0, (gpio_num_t) CONFIG_SDA_GPIO, (gpio_num_t) CONFIG_SCL_GPIO);
if (ds3231_initialization_status != ESP_OK) {
    ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
    
    while (1) { vTaskDelay(1); }
}
ds3231_enable_sqw(&dev, DS3231_4096HZ); // Squarewave |_|‾| for display DS3231_1HZ

  display.begin();
  display.setRotation(2);
  display.clearDisplayBuffer();

  display.setTextColor(BLACK);
  display.setFont(&Ubuntu_M8pt8b);

  while (1) { loop(); }
}


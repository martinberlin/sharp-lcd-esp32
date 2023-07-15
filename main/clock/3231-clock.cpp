#include "settings.h"
#include <Adafruit_SharpMem.h>
// RESEARCH FOR SPI HAT Cinwrite, PCB and Schematics            https://github.com/martinberlin/H-cinread-it895
// Note: This requires an IT8951 board and our Cinwrite PCB. It can be also adapted to work without it using an ESP32 (Any of it's models)
#include "ds3231.h"
struct tm rtcinfo;
// Non-Volatile Storage (NVS) - borrrowed from esp-idf/examples/storage/nvs_rw_value
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h> // sin cosine
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include <time.h>
#include <sys/time.h>
// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"
// Attention: Enabling slow CPU speed won't be able to sync the clock with WiFi
#include "esp_pm.h"
/* Enable CPU frequency changing depending on button press */
#define USE_CPU_PM_SPEED 1

typedef struct {
    int max_freq_mhz;         /*!< Maximum CPU frequency, in MHz */
    int min_freq_mhz;         /*!< Minimum CPU frequency to use when no locks are taken, in MHz */
    bool light_sleep_enable;  /*!< Enter light sleep when no locks are taken */
} esp_pm_config_t;
#define TEN_IN_SIXTH           (1000000)
#define SLOW_CPU_MHZ           10
#define SLOW_CPU_SPEED         (SLOW_CPU_MHZ * TEN_IN_SIXTH)
#define NORMAL_CPU_SPEED       (80 * TEN_IN_SIXTH)
uint32_t cpu_speed = NORMAL_CPU_SPEED;
// SHARP LCD Class
// Set the size of the display here, e.g. 128x128
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 128, 128);

// Correction is TEMP - this number (Used for wrist watches)
float ds3231_temp_correction = 5.0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0)
  #error "ESP_IDF version not supported. Please use IDF 4.4 or IDF v5.0-beta1"
#endif

// Fonts
#include <Ubuntu_M8pt8b.h>
#include <Ubuntu_M12pt8b.h>
#include <Ubuntu_M24pt8b.h>

// NVS non volatile storage
nvs_handle_t storage_handle;

/**
┌───────────────────────────┐
│ CLOCK configuration       │ Device wakes up each N minutes -> Not used but could be interesting for ultra-low consumption
└───────────────────────────┘
**/
#define DEEP_SLEEP_SECONDS 4

/**
┌───────────────────────────┐
│ NIGHT MODE configuration  │ Make the module sleep in the night to save battery power
└───────────────────────────┘
**/
// Leave NIGHT_SLEEP_START in -1 to never sleep. Example START: 22 HRS: 8  will sleep from 10PM till 6 AM
#define NIGHT_SLEEP_START 22
#define NIGHT_SLEEP_HRS   8
// sleep_mode=1 uses precise RTC wake up. RTC alarm pulls GPIO_RTC_INT low when triggered
// sleep_mode=0 wakes up every 10 min till NIGHT_SLEEP_HRS. Useful to log some sensors while epaper does not update
uint8_t sleep_mode = 0;
bool rtc_wakeup = true;
// sleep_mode=1 requires precise wakeup time and will use NIGHT_SLEEP_HRS+20 min just as a second unprecise wakeup if RTC alarm fails
// Needs menuconfig --> DS3231 Configuration -> Set clock in order to store this alarm once
uint8_t wakeup_hr = 7;
uint8_t wakeup_min= 1;

uint64_t USEC = 1000000;

uint8_t clock_screen = 1; // 1 clock   2 game of life   3 sleep
// Weekdays and months translatables (Select one only)
//#include <catala.h>
//#include <english.h>
#include <spanish.h>
//#include <deutsch.h>

// You have to set these CONFIG value using: idf.py menuconfig --> DS3231 Configuration
#if 0
#define CONFIG_SCL_GPIO		7
#define CONFIG_SDA_GPIO		15
#define	CONFIG_TIMEZONE		9
#define NTP_SERVER 		"pool.ntp.org"
#endif

esp_err_t ds3231_initialization_status = ESP_OK;
static const char *TAG = "LCD";

// I2C descriptor
i2c_dev_t dev;


extern "C"
{
    void app_main();
}

void delay(uint32_t millis) { vTaskDelay(pdMS_TO_TICKS(millis)); }

QueueHandle_t interputQueue;

// CPU frequency management function
uint8_t set_CPU_speed(uint32_t speed) {
    cpu_speed = speed;
    uint8_t err = ESP_OK;
    const int low_freq = SLOW_CPU_SPEED / TEN_IN_SIXTH;
    const int hi_freq = NORMAL_CPU_SPEED / TEN_IN_SIXTH;
    int _xt_tick_divisor = 0;
    esp_pm_config_t pm_config;
    switch (speed) {
        case SLOW_CPU_SPEED:
            pm_config.min_freq_mhz = low_freq;
            pm_config.max_freq_mhz = low_freq;
            err = esp_pm_configure(&pm_config);
            if (err == ESP_OK) {
                _xt_tick_divisor = SLOW_CPU_SPEED / configTICK_RATE_HZ;
            }
            break;
        case NORMAL_CPU_SPEED:
            pm_config.min_freq_mhz = hi_freq;
            pm_config.max_freq_mhz = hi_freq;
            err = esp_pm_configure(&pm_config);
            if (err == ESP_OK) {
                _xt_tick_divisor = NORMAL_CPU_SPEED / configTICK_RATE_HZ;
            }
            break;
        default:
            err = 0xFF;
            break;
    }
    return err;
}

static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    int pinNumber = (int)args;
    xQueueSendFromISR(interputQueue, &pinNumber, NULL);
}

uint16_t generateRandom(uint16_t max) {
    if (max>0) {
        srand(esp_timer_get_time());
        return rand() % max;
    }
    return 0;
}

void delay_ms(uint32_t period_ms) {
    sys_delay_ms(period_ms);
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    ESP_LOGI(TAG, "Your NTP Server is %s", CONFIG_NTP_SERVER);
    sntp_setservername(0, CONFIG_NTP_SERVER);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

static bool obtain_time(void)
{
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK( example_disconnect() );
    if (retry == retry_count) return false;
    return true;
}

void deep_sleep(uint16_t seconds_to_sleep) {
    // Turn off the 3.7 to 5V step-up and put all IO pins in INPUT mode
    uint8_t EP_CONTROL[] = {SHARP_SCK, SHARP_MOSI, SHARP_SS, LCD_EXTMODE, LCD_DISP};
    for (int io = 0; io < 5; io++) {
        gpio_set_level((gpio_num_t) EP_CONTROL[io], 0);
        gpio_set_direction((gpio_num_t) EP_CONTROL[io], GPIO_MODE_INPUT);
    }
    ESP_LOGI(pcTaskGetName(0), "DEEP_SLEEP_SECONDS: %d seconds to wake-up", seconds_to_sleep);
    esp_sleep_enable_timer_wakeup(seconds_to_sleep * USEC);
    esp_deep_sleep_start();
}


/**
 * @brief Turn back 1 hr in last sunday October or advance 1 hr in end of March for EU summertime
 * 
 * @param rtcinfo 
 * @param correction 
 */
void summertimeClock(tm rtcinfo, int correction) {
    struct tm time = {
        .tm_sec  = rtcinfo.tm_sec,
        .tm_min  = rtcinfo.tm_min,
        .tm_hour = rtcinfo.tm_hour + correction,
        .tm_mday = rtcinfo.tm_mday,
        .tm_mon  = rtcinfo.tm_mon,
        .tm_year = rtcinfo.tm_year,
        .tm_wday = rtcinfo.tm_wday
    };

    if (ds3231_set_time(&dev, &time) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not set time for summertime correction (%d)", correction);
        return;
    }
    ESP_LOGI(pcTaskGetName(0), "Set summertime correction time done");
}

void setClock()
{
    // obtain time over NTP
    ESP_LOGI(pcTaskGetName(0), "Connecting to WiFi and getting time over NTP.");
    if(!obtain_time()) {
        ESP_LOGE(pcTaskGetName(0), "Fail to getting time over NTP.");
        while (1) { vTaskDelay(1); }
    }

    // update 'now' variable with current time
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    now = now + (CONFIG_TIMEZONE*60*60);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(pcTaskGetName(0), "The current date/time is: %s", strftime_buf);

    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_sec=%d",timeinfo.tm_sec);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_min=%d",timeinfo.tm_min);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_hour=%d",timeinfo.tm_hour);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_wday=%d",timeinfo.tm_wday);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_mday=%d",timeinfo.tm_mday);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_mon=%d",timeinfo.tm_mon);
    ESP_LOGD(pcTaskGetName(0), "timeinfo.tm_year=%d",timeinfo.tm_year);

    printf("Setting tm_wday: %d\n\n", timeinfo.tm_wday);

    struct tm time = {
        .tm_sec  = timeinfo.tm_sec,
        .tm_min  = timeinfo.tm_min,
        .tm_hour = timeinfo.tm_hour,
        .tm_mday = timeinfo.tm_mday,
        .tm_mon  = timeinfo.tm_mon,  // 0-based
        .tm_year = timeinfo.tm_year + 1900,
        .tm_wday = timeinfo.tm_wday
    };

    if (ds3231_set_time(&dev, &time) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not set time.");
        while (1) { vTaskDelay(1); }
    }
    ESP_LOGI(pcTaskGetName(0), "Set initial date time done");

    display.setFont(&Ubuntu_M24pt8b);
    display.println("Initial date time\nis saved on RTC\n");

    display.printerf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    if (sleep_mode) {
        // Set RTC alarm. This won't work if RTC Int is not routed as input IO
        time.tm_hour = wakeup_hr;
        time.tm_min  = wakeup_min;
        display.println("RTC alarm set to this hour:");
        display.printerf("%02d:%02d", time.tm_hour, time.tm_min);
        ESP_LOGI((char*)"RTC ALARM", "%02d:%02d", time.tm_hour, time.tm_min);
        ds3231_clear_alarm_flags(&dev, DS3231_ALARM_2);
        // i2c_dev_t, ds3231_alarm_t alarms, struct tm *time1,ds3231_alarm1_rate_t option1, struct tm *time2, ds3231_alarm2_rate_t option2
        ds3231_set_alarm(&dev, DS3231_ALARM_2, &time, (ds3231_alarm1_rate_t)0,  &time, DS3231_ALARM2_MATCH_MINHOUR);
        ds3231_enable_alarm_ints(&dev, DS3231_ALARM_2);
    }
    // Wait some time to see if disconnecting all changes background color
    esp_restart();
}

// Round clock draw functions
uint16_t clock_x_shift = 2;
uint16_t clock_y_shift = 2;
uint16_t clock_radius = 120;
uint16_t maxx = 0;
uint16_t maxy = 0;
void secHand(uint8_t sec)
{
    int sec_radius = clock_radius-20;
    float O;
    int x = maxx/2+clock_x_shift;
    int y = maxy/2+clock_y_shift;
    /* determining the angle of the line with respect to vertical */
    O=(sec*(M_PI/30)-(M_PI/2)); 
    x = x+sec_radius*cos(O);
    y = y+sec_radius*sin(O);
    display.drawLine(maxx/2+clock_x_shift,maxy/2+clock_y_shift,x,y, BLACK);
}

void minHand(uint8_t min)
{
    int min_radius = 60;
    float O;
    int x = maxx/2+clock_x_shift;
    int y = maxy/2+clock_y_shift;
    O=(min*(M_PI/30)-(M_PI/2)); 
    x = x+min_radius*cos(O);
    y = y+min_radius*sin(O);
    display.drawLine(maxx/2+clock_x_shift,maxy/2+clock_y_shift,x,y, BLACK);
    display.drawLine(maxx/2+clock_x_shift,maxy/2-4+clock_y_shift,x,y, BLACK);
    display.drawLine(maxx/2+clock_x_shift,maxy/2+4+clock_y_shift,x+1,y, BLACK);
    display.drawLine(maxx/2+clock_x_shift,maxy/2+3+clock_y_shift,x+1,y-1, BLACK);
}

void hrHand(uint8_t hr, uint8_t min)
{
    uint16_t hand_radius = 30;
    float O;
    int x = maxx/2+clock_x_shift;
    int y = maxy/2+clock_y_shift;
    
    if(hr<=12)O=(hr*(M_PI/6)-(M_PI/2))+((min/12)*(M_PI/30));
    if(hr>12) O=((hr-12)*(M_PI/6)-(M_PI/2))+((min/12)*(M_PI/30));
    x = x+hand_radius*cos(O);
    y = y+hand_radius*sin(O);
    
    display.drawLine(maxx/2-1+clock_x_shift,maxy/2-3+clock_y_shift-1, x-1, y-1, BLACK);
    display.drawLine(maxx/2-3+clock_x_shift,maxy/2+3+clock_y_shift-1, x+1, y-1, BLACK);
    display.drawLine(maxx/2-1+clock_x_shift,maxy/2+clock_y_shift-1, x-1, y-1, BLACK);
    display.drawLine(maxx/2-1+clock_x_shift,maxy/2+clock_y_shift-1, x+1, y-1, BLACK);
    display.drawLine(maxx/2-2+clock_x_shift,maxy/2+clock_y_shift-1, x-1, y-1, BLACK);
    display.drawLine(maxx/2-2+clock_x_shift,maxy/2+clock_y_shift-1, x+1, y-1, BLACK);

    display.drawLine(maxx/2+2+clock_x_shift+1,maxy/2+3+clock_y_shift+1, x+1, y+1, BLACK);
    display.drawLine(maxx/2-2+clock_x_shift-2,maxy/2+3+clock_y_shift, x-1, y+1, BLACK);
    display.drawLine(maxx/2+2+clock_x_shift+1,maxy/2+3+clock_y_shift+2, x+1, y+2, BLACK);
    display.drawLine(maxx/2-2+clock_x_shift-2,maxy/2+3+clock_y_shift, x-1, y+2, BLACK);
    display.drawLine(maxx/2+2+clock_x_shift+1,maxy/2+3+clock_y_shift+3, x+1, y+3, BLACK);
    display.drawLine(maxx/2-2+clock_x_shift-2,maxy/2+3+clock_y_shift, x-1, y+3, BLACK);
}

void clockLayout(uint8_t hr, uint8_t min, uint8_t sec)
{
    // Circle in the middleRound clock dra
    display.fillCircle(maxx/2+clock_x_shift, maxy/2+clock_y_shift, 6, BLACK);
    // Draw hour hands
    hrHand(hr, min);
    minHand(min);
    secHand(sec);
}

// GAME of Life (Credits: John Conway)

//Current grid: width must be multiple of 8
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


void getClock() {
    if (ds3231_get_time(&dev, &rtcinfo) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not get time.");
    }
    // Get RTC date and time
    int16_t temp;
    // Just debug deepsleep        deep_sleep(DEEP_SLEEP_SECONDS);
    
    if (rtcinfo.tm_sec == 00) {
        if (ds3231_get_raw_temp(&dev, &temp) != ESP_OK) {
            ESP_LOGE(TAG, "Could not get temperature.");
            return;
        }
        nvs_set_i16(storage_handle, "rtc_temp", temp);
    }
    //ESP_LOGI("CLOCK", "\n%s\n%02d:%02d", weekday_t[rtcinfo.tm_wday], rtcinfo.tm_hour, rtcinfo.tm_min);
    display.clearDisplayBuffer();
    display.setTextColor(BLACK);

    switch (clock_screen)
    {
    case 1:
    {
    #if USE_CPU_PM_SPEED == 1
        if (cpu_speed != SLOW_CPU_SPEED) {
            set_CPU_speed(SLOW_CPU_SPEED);
            ESP_LOGI(TAG, "%d Mhz CPU", SLOW_CPU_SPEED);
        }
    #endif
        // Starting coordinates:
    uint16_t y_start = 40;
    uint16_t x_cursor = 10;
    
    // Print day number and month
    if (display.width() <= 200) {
        y_start = 20;
        x_cursor = 4;
        display.setFont(&Ubuntu_M8pt8b);
        display.setCursor(x_cursor, y_start);
    } else {
        display.setFont(&Ubuntu_M12pt8b);
        display.setCursor(x_cursor+20, y_start);
    }
    display.printerf("%s %d %s", weekday_t[rtcinfo.tm_wday], rtcinfo.tm_mday, month_t[rtcinfo.tm_mon]);

    // HH:MM
    y_start += 50;
    x_cursor = 1;
    display.setFont(&Ubuntu_M24pt8b);
    display.setTextColor(BLACK);
    display.setCursor(x_cursor, y_start);
    display.printerf("%02d:%02d", rtcinfo.tm_hour, rtcinfo.tm_min);
    // Seconds
    y_start = 92;
    x_cursor = display.width()-26;
    display.setFont(&Ubuntu_M8pt8b);
    display.setCursor(x_cursor, y_start);
    display.printerf("%02d", rtcinfo.tm_sec);

    y_start = 80;
    x_cursor = 4;
    display.drawRect(x_cursor, y_start, 120, 16, BLACK);
    display.fillRect(x_cursor, y_start+1, rtcinfo.tm_sec*2, 14, BLACK);

    // Print temperature
    if (display.width() <= 200) {
      x_cursor = 10;
      y_start = display.height()-10;
      display.setFont(&Ubuntu_M12pt8b);
    } else {
      y_start = display.height()-30;
      x_cursor+= 26;
      display.setFont(&Ubuntu_M24pt8b);
    }

    // Read temperature from NVS
    nvs_get_i16(storage_handle, "rtc_temp", &temp);
    double temperature = temp * 0.25;
    
    display.setCursor(x_cursor, y_start);
    display.printerf("%.1f°C", temperature - ds3231_temp_correction);
    }
        break;
    
    case 2:
        #if USE_CPU_PM_SPEED == 1
        if (cpu_speed != SLOW_CPU_SPEED) {
            set_CPU_speed(SLOW_CPU_SPEED);
            ESP_LOGI(TAG, "%d Mhz CPU", SLOW_CPU_SPEED);
        }
        #endif
        clockLayout(rtcinfo.tm_hour, rtcinfo.tm_min, rtcinfo.tm_sec);
        break;
    case 3:
        #if USE_CPU_PM_SPEED == 1
        if (cpu_speed != NORMAL_CPU_SPEED) {
            set_CPU_speed(NORMAL_CPU_SPEED);
            ESP_LOGI(TAG, "%d Mhz CPU", NORMAL_CPU_SPEED);
        }
        #endif
        /**
         * @brief   Game of life. Adapted from delhoume example (Check games/life.cpp)
         * @author  delhoume     (github)
         */
        initGrid();
        // Takes a bit long but is worth the show
        for (uint16_t gen = 0; gen < maxGenerations; gen++) {
            computeNewGeneration();
            delay(5);
            display.refresh();
        }
    break;
    }
    
    display.refresh();
    // DEBUG
    /* ESP_LOGI(pcTaskGetName(0), "%04d-%02d-%02d %02d:%02d:%02d, Week day:%d, %.2f °C", 
        rtcinfo.tm_year, rtcinfo.tm_mon + 1,
        rtcinfo.tm_mday, rtcinfo.tm_hour, rtcinfo.tm_min, rtcinfo.tm_sec, rtcinfo.tm_wday, temp); */
    // Enable something like this if you want to print your clock only each x SECONDS
    //delay(2000);
    //deep_sleep(DEEP_SLEEP_SECONDS);
}

void display_print_sleep_msg() {
    nvs_set_u8(storage_handle, "sleep_msg", 1);

    // Wait until board is fully powered
    delay_ms(80);
    display.begin();
    
    display.setFont(&Ubuntu_M12pt8b);
    unsigned int color = WHITE;

    display.fillRect(0, 0, display.width() , display.height(), color);
    uint16_t y_start = display.height()/2;
    display.setCursor(10, y_start);
    display.print("NIGHT SLEEP");
    display.setCursor(10, y_start+44);
    display.printerf("%d:00 + %d Hrs.", NIGHT_SLEEP_START, NIGHT_SLEEP_HRS);

    float temp;
    if (ds3231_get_temp_float(&dev, &temp) == ESP_OK) {
        y_start+= 384;
        display.setTextColor(BLACK);
        display.setCursor(100, y_start);
        display.printerf("%.2f C", temp);
    }
    delay_ms(180);
}

// NVS variable to know how many times the ESP32 wakes up
int16_t nvs_boots = 0;
uint8_t sleep_flag = 0;
uint8_t sleep_msg = 0;
// Flag to know if summertime is active
uint8_t summertime = 0;

// Calculates if it's night mode
bool calc_night_mode(struct tm rtcinfo) {
    struct tm time_ini, time_rtc;
    // Night sleep? (Save battery)
    nvs_get_u8(storage_handle, "sleep_flag", &sleep_flag);
    //printf("sleep_flag:%d\n", sleep_flag);

    if (rtcinfo.tm_hour >= NIGHT_SLEEP_START && sleep_flag == 0) {
        // Save actual time struct in NVS
        nvs_set_u8(storage_handle, "sleep_flag", 1);
        
        nvs_set_u16(storage_handle, "nm_year", rtcinfo.tm_year);
        nvs_set_u8(storage_handle, "nm_mon",  rtcinfo.tm_mon);
        nvs_set_u8(storage_handle, "nm_mday", rtcinfo.tm_mday);
        nvs_set_u8(storage_handle, "nm_hour", rtcinfo.tm_hour);
        nvs_set_u8(storage_handle, "nm_min", rtcinfo.tm_min);
        printf("night mode nm_* time saved %d:%d\n", rtcinfo.tm_hour, rtcinfo.tm_min);
        return true;
    }
    if (sleep_flag == 1) {
        uint8_t tm_mon,tm_mday,tm_hour,tm_min;
        uint16_t tm_year;
        nvs_get_u16(storage_handle, "nm_year", &tm_year);
        nvs_get_u8(storage_handle, "nm_mon", &tm_mon);
        nvs_get_u8(storage_handle, "nm_mday", &tm_mday);
        nvs_get_u8(storage_handle, "nm_hour", &tm_hour);
        nvs_get_u8(storage_handle, "nm_min", &tm_min);

        struct tm time_ini_sleep = {
            .tm_sec  = 0,
            .tm_min  = tm_min,
            .tm_hour = tm_hour,
            .tm_mday = tm_mday,
            .tm_mon  = tm_mon,  // 0-based
            .tm_year = tm_year - 1900,
        };
        // update 'rtcnow' variable with current time
        char strftime_buf[64];

        time_t startnm = mktime(&time_ini_sleep);
        // RTC stores year 2022 as 122
        rtcinfo.tm_year -= 1900;
        time_t rtcnow = mktime(&rtcinfo);

        localtime_r(&startnm, &time_ini);
        localtime_r(&rtcnow, &time_rtc);
        // Some debug to see what we compare
        if (false) {
            strftime(strftime_buf, sizeof(strftime_buf), "%F %r", &time_ini);
            ESP_LOGI(pcTaskGetName(0), "INI datetime is: %s", strftime_buf);
            strftime(strftime_buf, sizeof(strftime_buf), "%F %r", &time_rtc);
            ESP_LOGI(pcTaskGetName(0), "RTC datetime is: %s", strftime_buf);
        }
        // Get the time difference
        double timediff = difftime(rtcnow, startnm);
        uint16_t wake_seconds = NIGHT_SLEEP_HRS * 60 * 60;
        ESP_LOGI(pcTaskGetName(0), "Time difference is:%f Wait till:%d seconds", timediff, wake_seconds);
        if (timediff >= wake_seconds) {
            nvs_set_u8(storage_handle, "sleep_flag", 0);
        } else {
            return true;
        }
        // ONLY Debug and testing
        //nvs_set_u8(storage_handle, "sleep_flag", 0);
    }
  return false;
}

void wakeup_cause()
{
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_EXT0: {
            printf("Wake up from ext0\n");
            break;
        }
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = 0; // Check this on C3:  esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("Wake up from GPIO %d\n", pin);
            } else {
                printf("Wake up from GPIO\n");
            }

            rtc_wakeup = true;
            // Woke up from RTC, clear alarm flag
            ds3231_clear_alarm_flags(&dev, DS3231_ALARM_2);
            break;
        }
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("Wake up from timer\n");
            break;
        }
        default:
            break;
    }
}

TaskHandle_t sqw_task;

void sqw_clear(void* pvParameters) {
  for (;;)
  {
    #if CONFIG_GET_CLOCK
        getClock();
    #endif
      //ds3231_clear_alarm_flags(&dev, DS3231_ALARM_1);
      delay(1000);
  }
}

void button_task(void *params)
{
    int pinNumber, count = 0;
    while (true)
    {
        if (xQueueReceive(interputQueue, &pinNumber, portMAX_DELAY))
        {
            switch (pinNumber) {
                case 8:
                    clock_screen = 1;
                break;
                case 4:
                    clock_screen = 2;
                break;
                case 10:
                    clock_screen = 3;
                break;
            }
            printf("GPIO %d was pressed %d times. The state is %d\n", pinNumber, count++, gpio_get_level((gpio_num_t)pinNumber));
        }
    }
}

void app_main()
{
    // Display pins config
    gpio_set_direction(LCD_EXTMODE, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_DISP, GPIO_MODE_OUTPUT);// Display On(High)/Off(Low) 
    gpio_set_level(LCD_DISP, 1);
    gpio_set_level(LCD_EXTMODE, 1); // Using Ext com in HIGH mode-> Signal sent by RTC at 1 Hz

    // Buttons configuration
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // interrupt of NEG edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // bit mask of the pins, check settings.h
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // enable pull-up mode (Not needed if there are HW pull-ups)
    io_conf.pull_up_en = (gpio_pullup_t)1;
    gpio_config(&io_conf);

    interputQueue = xQueueCreate(10, sizeof(int));
    xTaskCreate(button_task, "button_task", 2048, NULL, 1, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PCB_BUTTON_1, gpio_interrupt_handler, (void *)PCB_BUTTON_1);
    gpio_isr_handler_add(PCB_BUTTON_2, gpio_interrupt_handler, (void *)PCB_BUTTON_2);
    gpio_isr_handler_add(PCB_BUTTON_3, gpio_interrupt_handler, (void *)PCB_BUTTON_3);

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_open("storage", NVS_READWRITE, &storage_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    // Initialize RTC
    ds3231_initialization_status = ds3231_init_desc(&dev, I2C_NUM_0, (gpio_num_t) CONFIG_SDA_GPIO, (gpio_num_t) CONFIG_SCL_GPIO);
    if (ds3231_initialization_status != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
        while (1) { vTaskDelay(1); }
    }

    #if CONFIG_SET_CLOCK
       setClock();
    #endif

  // Initialize RTC SQW
  if (true)  {
    ds3231_enable_sqw(&dev, DS3231_4096HZ); // Squarewave |_|‾| for display DS3231_1HZ
   
    if (ds3231_get_time(&dev, &rtcinfo) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not get time.");
    } 
    if (rtcinfo.tm_mday == 1 && rtcinfo.tm_year == 2000 && strcmp(CONFIG_EXAMPLE_WIFI_PASSWORD,"")!=0) {
        setClock();
    }

    // Maximum power saving (But slower WiFi which we use only to callibrate RTC)
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    // Determine wakeup cause and clear RTC alarm flag
    wakeup_cause(); // Needs I2C dev initialized

    // Handle clock update for EU summertime
    #if SYNC_SUMMERTIME
    nvs_get_u8(storage_handle, "summertime", &summertime);
    // IMPORTANT: Do not forget to set summertime initially to 0 (leave it ready before march) or 1 (between March & October)
    //nvs_set_u8(storage_handle, "summertime", 0);
   
    // EU Summertime
    // Last sunday of March -> Forward 1 hour
    if (rtcinfo.tm_mday > 24 && rtcinfo.tm_mon == 2 && rtcinfo.tm_wday == 0 && rtcinfo.tm_hour == 8 && summertime == 0) {
        nvs_set_u8(storage_handle, "summertime", 1);
        summertimeClock(rtcinfo, 1);
    }
    // Last sunday of October -> Back 1 hour
    if (rtcinfo.tm_mday > 24 && rtcinfo.tm_mon == 9 && rtcinfo.tm_wday == 0 && rtcinfo.tm_hour == 8 && summertime == 1) {
        nvs_set_u8(storage_handle, "summertime", 0);
        summertimeClock(rtcinfo, -1);
    }
    #endif

    // Read stored
    nvs_get_i16(storage_handle, "boots", &nvs_boots);

    ESP_LOGI(TAG, "-> NVS Boot count: %d", nvs_boots);
    nvs_boots++;
    // Set new value
    nvs_set_i16(storage_handle, "boots", nvs_boots);
    // Reset sleep msg flag so it prints it again before going to sleep
    if (sleep_msg) {
        nvs_set_u8(storage_handle, "sleep_msg", 0);
    }

    display.begin();
    display.setRotation(2);
    ESP_LOGI(TAG, "CONFIG_SCL_GPIO = %d", CONFIG_SCL_GPIO);
    ESP_LOGI(TAG, "CONFIG_SDA_GPIO = %d", CONFIG_SDA_GPIO);
    ESP_LOGI(TAG, "CONFIG_TIMEZONE= %d", CONFIG_TIMEZONE);

    display.setFont(&Ubuntu_M24pt8b);
    maxx = display.width();
    maxy = display.height();
   
   // No need to do this with SquareWave in RTC
   xTaskCreatePinnedToCore(
    sqw_clear, // Task function
    "sqw_task",// name of task
    10000,     // Stack size of task
    NULL,      // parameter of the task
    1,         // priority of the task
    &sqw_task, // Task handle to keep track of created task
    0);    // pin task to core 0
  }
}

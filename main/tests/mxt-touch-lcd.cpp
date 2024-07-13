
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Adafruit_SharpMem.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

// Error handling
#include "esp_err.h"
#include "esp_check.h"
#include "driver/rtc_io.h"

#include "ds3231.h"

// MXT MaxTouch initialization
#define MXT144_ADDR 0x4A
typedef struct mxt_data_tag {
    uint16_t t2_encryption_status_address;
    uint16_t t5_message_processor_address;
    uint16_t t5_max_message_size;
    uint16_t t6_command_processor_address;
    uint16_t t7_powerconfig_address;
    uint16_t t8_acquisitionconfig_address;
    uint16_t t44_message_count_address;
    uint16_t t46_cte_config_address;
    uint16_t t100_multiple_touch_touchscreen_address;
    uint16_t t100_first_report_id;
} MXTDATA;

typedef struct mxt_object_tag {
    uint8_t type;
    uint16_t position;
    uint8_t size_minus_one;
    uint8_t instances_minus_one;
    uint8_t report_ids_per_instance;
} MXTOBJECT;

MXTDATA _mxtdata;

#define MXT_MESSAGE_SIZE 6

// LCD SPI  Ios for 
#define SHARP_SCK  1
#define SHARP_MOSI 2
#define SHARP_SS   3

#define TOUCH_INT  6

// I2C int the ESP32-S3 board
#define SDA_GPIO GPIO_NUM_7
#define SCL_GPIO GPIO_NUM_8
i2c_dev_t dev;

// I2C common protocol defines
#define WRITE_BIT                          I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                           I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                       0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                      0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                            0x0              /*!< I2C ack value */
#define NACK_VAL                           0x1              /*!< I2C nack value */


#define BLACK 0
#define WHITE 1

// External COM Inversion Signal
#define LCD_EXTMODE GPIO_NUM_14
#define LCD_DISP    GPIO_NUM_4

// Deepsleep tests
#define MICROS_TO_SECOND 1000000

// Set the size of the display here, e.g. WIDTHxHEIGHT
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 400, 240);

extern "C"
{
   void app_main();
}


void delay(uint32_t millis) { vTaskDelay(millis / portTICK_PERIOD_MS); }



int I2CReadRegister16(uint8_t i2c_addr, uint16_t u16Register, uint8_t *pData, int iLen)
{
  int i = 0;

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  // first, send device address (indicating write) & register to be written
  i2c_master_write_byte(cmd, ( i2c_addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  // send register we want
  i2c_master_write_byte(cmd, (uint8_t)(u16Register & 0xff), ACK_CHECK_EN);
  i2c_master_write_byte(cmd, (uint8_t)(u16Register>>8), ACK_CHECK_EN);
  i2c_master_stop(cmd);

  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);

  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  // now send device address (indicating read) & read data
  i2c_master_write_byte(cmd, ( i2c_addr << 1 ) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read(cmd, pData, iLen, (i2c_ack_type_t) ACK_VAL);
  
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret;
} /* I2CReadRegister16() */

// Initalize the MXT144 - an overly complicated mess of a touch sensor
int initMXT(void)
{
uint8_t ucTemp[32];
int i, iObjCount, iReportID;
uint16_t u16, u16Offset;

// Read information block (first 7 bytes of address space)
   I2CReadRegister16(MXT144_ADDR, 0, ucTemp, 7);
   iObjCount = ucTemp[6];
   if (iObjCount < 1 || iObjCount >64) { // invalid number of items
      printf("INVALID object count = %d\n", iObjCount);

      return ESP_FAIL;
   }
   u16Offset = 7; // starting offset of first object
   // Read the objects one by one to get the memory offests to the info we will need
   iReportID = 1;
   printf("object count = %d\n", iObjCount);
   return ESP_OK;
}


void app_main() {
  printf("Hello LCD, example to test MXT-Touch\n\n");
  gpio_set_direction(LCD_EXTMODE, GPIO_MODE_OUTPUT);
  gpio_set_direction(LCD_DISP, GPIO_MODE_OUTPUT);// Display On(High)/Off(Low) 
  gpio_set_level(LCD_DISP, 1);
  gpio_set_level(LCD_EXTMODE, 1); // Using Ext com in HIGH mode-> Signal sent by RTC at 1 Hz

  // Initialize RTC and SQW
  if (ds3231_init_desc(&dev, I2C_NUM_0, SDA_GPIO, SCL_GPIO) != ESP_OK) {
      ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
      while (1) { vTaskDelay(1); }
  }

  // Enable SQW for LCD:
  ds3231_enable_sqw(&dev, DS3231_1HZ);

  // Begin initializing touch
  initMXT();

  display.begin();
  display.clearDisplay();
  display.setRotation(1);
  display.setCursor(10,20);
  display.setTextSize(3);
  display.setTextColor(BLACK);

  display.print("Hello SHARP");  
  display.refresh();
  delay(500);

    const gpio_config_t config = {
        .pin_bit_mask = BIT(TOUCH_INT),
        .mode = GPIO_MODE_INPUT,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
    esp_sleep_enable_ext1_wakeup((gpio_num_t)TOUCH_INT, ESP_EXT1_WAKEUP_ALL_LOW);

    printf("Enabling GPIO wakeup on pins GPIO %d\n", TOUCH_INT);
    delay(5000);
    printf("start deep_sleep\n");
    delay(1);
    esp_deep_sleep(10 * MICROS_TO_SECOND);
}
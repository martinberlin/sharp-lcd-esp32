// Just a quick I2C Address scan. This was only to test Seeed sensor
#ifdef SEED_HM_SENSOR
  #define ENABLE_SEEED_GPIO GPIO_NUM_48
#endif
// Please define the target where you are flashing this
// only one should be true:
#define TARGET_ESP32_DEFAULT false
#define TARGET_C3_WATCH true

#if TARGET_ESP32_DEFAULT
    #define SDA_GPIO 21
    #define SCL_GPIO 22
#endif
#if TARGET_C3_WATCH
    // LCD watch PCB
    #define SDA_GPIO 5
    #define SCL_GPIO 1
#endif

#define I2C_MASTER_FREQ_HZ 100000                     /*!< I2C master clock frequency */
#define I2C_SCLK_SRC_FLAG_FOR_NOMAL       (0)         /*!< Any one clock source that is available for the specified frequency may be choosen*/

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  #define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
// Enable on HIGH 5V boost converter
#define GPIO_ENABLE_5V GPIO_NUM_38

static const char *TAG = "i2c test";

extern "C"
{
    void app_main();
}

// setup i2c master
static esp_err_t i2c_master_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA_GPIO;
    conf.scl_io_num = SCL_GPIO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(I2C_NUM_0, &conf);
    return i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

void app_main()
{
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_19, 1);

    ESP_LOGI(TAG, "SCL_GPIO = %d", SCL_GPIO);
    ESP_LOGI(TAG, "SDA_GPIO = %d", SDA_GPIO);


#ifdef SEED_HM_SENSOR
   gpio_set_level(ENABLE_SEEED_GPIO, 1);
#endif
    #if TARGET_S3_CINWRITE
        gpio_set_direction(GPIO_ENABLE_5V ,GPIO_MODE_OUTPUT);
        // Turn on the 3.7 to 5V step-up
        gpio_set_level(GPIO_ENABLE_5V, 1);
    #endif
    
    // i2c init & scan
    if (i2c_master_init() != ESP_OK)
        ESP_LOGE(TAG, "i2c init failed\n");

    vTaskDelay(150 / portTICK_PERIOD_MS);
     printf("i2c scan: \n");
     for (uint8_t i = 1; i < 127; i++)
     {
        int ret;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, 1);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 100 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    
        if (ret == ESP_OK)
        {
            printf("Found device at: 0x%2x\n", i);
        }
    }
}

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_private/esp_clk.h"
#include "esp_timer.h"

typedef struct {
    int max_cpu_freq;         /*!< Maximum CPU frequency, in MHz */
    int min_cpu_freq;         /*!< Minimum CPU frequency to use when no locks are taken, in MHz */
    bool light_sleep_enable;  /*!< Enter light sleep when no locks are taken */
} esp_pm_config_t;

void switch_freq(int mhz)
{
    esp_pm_config_t pm_config = {
    .max_cpu_freq = mhz,
    .min_cpu_freq = 40
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
    printf("Waiting for frequency to be set to %d MHz...\n", mhz);
    while (esp_clk_cpu_freq() / 1000000 != mhz) {
        vTaskDelay(10);
    }
    printf("Frequency is set to %d MHz\n", mhz);
}

void app_main()
{
    switch_freq(80);
    switch_freq(160);
    
}
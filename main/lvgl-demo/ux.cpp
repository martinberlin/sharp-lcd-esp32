// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

#include "lv_conf.h"
#include "lvgl.h"
#include "SHARP_MIP.h" // Should be included by LVGL

extern "C"
{
    void app_main();
}

void app_main()
{
   static lv_disp_draw_buf_t draw_buf_dsc_2;
    static lv_color_t buf_2_1[LV_HOR_RES_MAX * 10];                        /*A buffer for 10 rows*/
    static lv_color_t buf_2_2[LV_HOR_RES_MAX * 10];                        /*An other buffer for 10 rows*/
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, LV_HOR_RES_MAX * 10);   /*Initialize the display buffer*/

    static lv_disp_drv_t disp_drv;                     /*A variable to hold the drivers. Can be local variable*/
    lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
        /*Set the resolution of the display*/
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    // LCD Callback functions
    sharp_mip_init();
    disp_drv.flush_cb = sharp_mip_flush;   /*Set a flush callback to draw to the display*/
    disp_drv.rounder_cb = sharp_mip_rounder;
    disp_drv.set_px_cb = sharp_mip_set_px;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc_2;

    /*Finally register the driver*/
    // FAILS here when tries to allocate memory in the ESP32
    lv_disp_drv_register(&disp_drv);
}
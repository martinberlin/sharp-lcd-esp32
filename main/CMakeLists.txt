idf_component_register(SRCS 
#"lvgl-demo/ux.cpp"
"tests/lcd-touch.cpp"
    #"tests/lcd-only.cpp"
    #"tests/hello-lcd.cpp"
    #"tests/i2c-scan.cpp"
    #"tests/pm-cpu-speed.c"
    #"clock/3231-clock.cpp"
    #"games/life.cpp"

    INCLUDE_DIRS translations
                 settings

    REQUIRES sharp-lcd
             nvs_flash
             rtc
             esp_event
             esp_wifi
             esp_timer
             fonts
             protocol_examples_common
             touch
             #lvgl
             #lvgl_esp32_drivers
)
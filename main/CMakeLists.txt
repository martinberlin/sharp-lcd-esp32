idf_component_register(SRCS 
    #"tests/hello-lcd.cpp"
    #"tests/i2c-scan.cpp"
    "clock/3231-clock.cpp"

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
)
# Make sure to setup your I2C GPIOs in menuconfig -> DS3231 Configuration
set(srcs 
    "ds3231/main/ds3231.c"
    # Shared between both
    "i2cdev.c"
)

idf_component_register(SRCS ${srcs}
                    REQUIRES driver
INCLUDE_DIRS "ds3231/main"
             "include"
)
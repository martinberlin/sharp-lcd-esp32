idf_component_register(
    SRCS "tests/hello-lcd.cpp"
    #SRCS "tests/i2c-scan.cpp"

    REQUIRES "sharp-lcd" 
             "rtc"
)
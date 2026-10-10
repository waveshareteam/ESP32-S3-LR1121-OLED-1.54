idf_component_register(
    REQUIRES esp32_s3_lr1121_oled_1_54_support
SRCS "i2ctools_example_main.c"
                            "cmd_i2ctools.c"
                       PRIV_REQUIRES fatfs esp_driver_i2c console
                       INCLUDE_DIRS ".")

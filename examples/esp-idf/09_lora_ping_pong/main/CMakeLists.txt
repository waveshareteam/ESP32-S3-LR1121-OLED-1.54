# register the component and set "RadioLib", "esp_timer" and "driver" as required
idf_component_register(
SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp32_s3_lr1121_oled_1_54_support RadioLib esp_timer esp_driver_spi esp_driver_gpio nvs_flash)

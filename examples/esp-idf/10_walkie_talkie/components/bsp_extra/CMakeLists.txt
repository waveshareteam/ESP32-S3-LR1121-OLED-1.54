idf_component_register(
    SRCS
        "src/bsp_board_extra.c"
    INCLUDE_DIRS
        "include"
    REQUIRES driver
    PRIV_REQUIRES esp_timer fatfs esp_psram esp_mm esp_driver_ledc
)
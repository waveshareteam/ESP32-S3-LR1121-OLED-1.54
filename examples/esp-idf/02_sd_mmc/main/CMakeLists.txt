set(srcs "sd_card_example_main.c")

idf_component_register(
SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES esp32_s3_lr1121_oled_1_54_support fatfs
                       WHOLE_ARCHIVE)

if(NOT CONFIG_SOC_SDMMC_HOST_SUPPORTED)
    fail_at_build_time(sdmmc ""
                             "Only ESP32 and ESP32-S3 targets are supported."
                             "Please refer README.md for more details")
endif()

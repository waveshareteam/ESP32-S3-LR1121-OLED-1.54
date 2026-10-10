idf_component_register(
    REQUIRES esp32_s3_lr1121_oled_1_54_support
SRCS "main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS ""
                       WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os)

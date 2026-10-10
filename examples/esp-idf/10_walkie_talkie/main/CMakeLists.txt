file(GLOB IMAGE_SRCS "ui/image/*.c")
list(APPEND IMAGE_SRCS "${CMAKE_CURRENT_LIST_DIR}/ui/image/GFSK.c")
list(REMOVE_DUPLICATES IMAGE_SRCS)

idf_component_register(
    REQUIRES esp32_s3_lr1121_oled_1_54_support nvs_flash esp32_codec2 esp_hw_support

    SRCS
        "app/app_state.cpp"
        "app/audio_pipeline.cpp"
        "app/peripheral_guard.cpp"
        "app/radio_device.cpp"
        "app/radio_runtime.cpp"
        "app/radio_settings.cpp"
        "ui/ui.cpp"
        "main.cpp"
        ${IMAGE_SRCS}
    INCLUDE_DIRS "." "app" "ui"
    WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os)

# spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

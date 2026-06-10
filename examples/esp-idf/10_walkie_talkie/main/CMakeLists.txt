file(GLOB IMAGE_SRCS "ui/image/*.c")

idf_component_register(
    SRCS "ui/ui.cpp" "main.cpp" ${IMAGE_SRCS}
    INCLUDE_DIRS "." "ui"
    WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os)

# spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

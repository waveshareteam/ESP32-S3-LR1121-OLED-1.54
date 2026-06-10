idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash bsp_extra
                       INCLUDE_DIRS ""
                       WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os)
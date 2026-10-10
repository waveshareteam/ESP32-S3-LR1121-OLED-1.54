# Application-local product glue; hardware values come from the local board YAML.
set(board_requires waveshare__waveshare_boards esp_board_manager esp_driver_gpio esp_driver_i2c esp_driver_spi
                   esp_driver_i2s esp_driver_sdmmc fatfs sdmmc esp_timer)
# Allow dependency resolution before bmgr's first generation. Once generated,
# require the board definitions explicitly so minimal builds include them too.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../gen_bmgr_codes/CMakeLists.txt")
    list(APPEND board_requires gen_bmgr_codes)
endif()

idf_component_register(
    SRCS "setup_device.c" "board_sensors.c" "board_display.c" "board_audio.c" "board_audio_player.c"
    INCLUDE_DIRS "include"
    REQUIRES ${board_requires}
    WHOLE_ARCHIVE)

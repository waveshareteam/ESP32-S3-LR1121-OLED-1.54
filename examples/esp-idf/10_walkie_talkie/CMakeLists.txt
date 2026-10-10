# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# This radio/audio application has no Wi-Fi or Bluetooth dependency.
idf_build_set_property(MINIMAL_BUILD ON)
include("${CMAKE_CURRENT_LIST_DIR}/../../../config/esp_idf_board_compat.cmake")
project(10_walkie_talkie)
include("${CMAKE_CURRENT_LIST_DIR}/../../../config/esp_idf_audio_compat.cmake")

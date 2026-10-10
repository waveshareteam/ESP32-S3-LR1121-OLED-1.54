cmake_minimum_required(VERSION 3.16)

# include the top-level cmake
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# name the project something nice
include("${CMAKE_CURRENT_LIST_DIR}/../../../config/esp_idf_board_compat.cmake")
project(11_lorawan_starter)

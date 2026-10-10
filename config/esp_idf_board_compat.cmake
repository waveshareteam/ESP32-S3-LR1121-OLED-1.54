# Check the selected product before project(). Board Manager adds the selected
# board directory to generated source and include paths even for YAML-only
# boards; normalize those paths so repeated bmgr generation stays warning-free.
function(board_fix_yaml_board_paths)
    set(board "esp32_s3_lr1121_oled_1_54_xf")
    set(board_dir "${CMAKE_CURRENT_SOURCE_DIR}/components/esp32_s3_lr1121_oled_1_54_support/boards/${board}")
    set(generated "${CMAKE_CURRENT_SOURCE_DIR}/components/gen_bmgr_codes/CMakeLists.txt")
    if(NOT EXISTS "${generated}")
        return()
    endif()
    # Board Manager also discovers other products from managed board packs.
    # Reject their generated drivers before component dependency resolution.
    set(defaults "${CMAKE_CURRENT_SOURCE_DIR}/components/gen_bmgr_codes/board_manager.defaults")
    if(NOT EXISTS "${defaults}")
        message(FATAL_ERROR "Incomplete generated board configuration. Run: idf.py bmgr -b ${board}")
    endif()
    file(STRINGS "${defaults}" selected_board REGEX "^CONFIG_ESP_BOARD_NAME=")
    if(NOT selected_board STREQUAL "CONFIG_ESP_BOARD_NAME=\"${board}\"")
        message(FATAL_ERROR
            "Wrong generated board (${selected_board}). This product requires ${board}. "
            "Run: idf.py bmgr -b ${board}")
    endif()
    file(GLOB board_files RELATIVE "${board_dir}" "${board_dir}/*")
    list(SORT board_files)
    if(NOT board_files STREQUAL "board_devices.yaml;board_info.yaml;board_peripherals.yaml")
        message(FATAL_ERROR "YAML-only board normalization requires exactly the three product YAML files")
    endif()
    file(READ "${generated}" original)
    string(REPLACE " \"../esp32_s3_lr1121_oled_1_54_support/boards/${board}\"" "" normalized "${original}")
    if(NOT normalized STREQUAL original)
        file(WRITE "${generated}" "${normalized}")
    endif()
endfunction()

board_fix_yaml_board_paths()

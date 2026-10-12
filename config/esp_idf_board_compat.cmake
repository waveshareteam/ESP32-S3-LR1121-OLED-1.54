# Check the selected product before project(). Board Manager adds the selected
# board directory to generated source and include paths even for YAML-only
# boards; normalize those paths so repeated bmgr generation stays warning-free.
function(board_fix_yaml_board_paths)
    set(board "esp32_s3_lr1121_oled_1_54_xf")
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
    file(READ "${generated}" original)
    set(normalized "${original}")
    set(board_available FALSE)
    foreach(relative IN ITEMS
            "../esp32_s3_lr1121_oled_1_54_support/boards/${board}"
            "../../managed_components/waveshare__waveshare_boards/boards/${board}")
        string(FIND "${original}" "\"${relative}\"" found)
        set(board_dir "${CMAKE_CURRENT_SOURCE_DIR}/components/gen_bmgr_codes/${relative}")
        if(EXISTS "${board_dir}" OR NOT found EQUAL -1)
            set(board_available TRUE)
            file(GLOB board_files RELATIVE "${board_dir}" "${board_dir}/*")
            list(SORT board_files)
            if(NOT board_files STREQUAL "board_devices.yaml;board_info.yaml;board_peripherals.yaml")
                message(FATAL_ERROR "YAML-only board normalization requires exactly the three product YAML files; review the resolved board pack")
            endif()
            string(REPLACE " \"${relative}\"" "" normalized "${normalized}")
        endif()
    endforeach()
    if(NOT board_available)
        message(FATAL_ERROR "Product board profile is missing; resolve dependencies and run: idf.py bmgr -b ${board}")
    endif()
    if(NOT normalized STREQUAL original)
        file(WRITE "${generated}" "${normalized}")
    endif()
endfunction()

board_fix_yaml_board_paths()

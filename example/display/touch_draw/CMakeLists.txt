cmake_minimum_required(VERSION 3.22)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
# Board defaults must match the chip selected by set-target.
set(board_defaults "${CMAKE_CURRENT_LIST_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(EXISTS "${board_defaults}")
    file(STRINGS "${board_defaults}" board_target_line REGEX "^CONFIG_IDF_TARGET=")
    string(REGEX REPLACE "^CONFIG_IDF_TARGET=\"([^\"]+)\"$" "\\1" board_target "${board_target_line}")
    # BMGR may remove sdkconfig and the CMake cache when selecting a board.
    set(project_sdkconfig "${SDKCONFIG}")
    if(NOT project_sdkconfig)
        set(project_sdkconfig "${CMAKE_CURRENT_LIST_DIR}/sdkconfig")
    endif()
    if(NOT IDF_TARGET AND NOT DEFINED ENV{IDF_TARGET} AND NOT EXISTS "${project_sdkconfig}")
        set(IDF_TARGET "${board_target}")
    endif()
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(EXISTS "${board_defaults}")
    if(IDF_TARGET STREQUAL board_target)
        # Apply example defaults after board defaults (fonts, FS and partitioning).
        set(SDKCONFIG_DEFAULTS "${board_defaults}" "sdkconfig.defaults")
    else()
        list(APPEND EXCLUDE_COMPONENTS gen_bmgr_codes)
    endif()
endif()

project(touch_draw)

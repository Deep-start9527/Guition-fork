# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/main/project_include.cmake")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(demo)

if(CONFIG_GUITION_BOARD_TOUCH_ADAPTER)
    # Board setup sources compiled by BMGR use the Guition touch adapter API.
    idf_component_get_property(board_setup_lib gen_bmgr_codes COMPONENT_LIB)
    idf_component_get_property(guition_board_lib guition-jc__guition_boards COMPONENT_LIB)
    target_link_libraries(${board_setup_lib} PUBLIC ${guition_board_lib})
endif()

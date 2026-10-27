# Keep LVGL, the adapter and the application on the same pixel layout.
# LVGL 9 also exposes the legacy depth macro, which can have a saved value.
if(CONFIG_DEMO_LCD_COLOR_RGB888)
    idf_build_set_property(COMPILE_DEFINITIONS
        "LV_COLOR_FORMAT_DEFAULT=LV_COLOR_FORMAT_RGB888;LV_COLOR_DEPTH=24" APPEND)
else()
    idf_build_set_property(COMPILE_DEFINITIONS
        "LV_COLOR_FORMAT_DEFAULT=LV_COLOR_FORMAT_RGB565;LV_COLOR_DEPTH=16" APPEND)
endif()

# main depends on the partition_table component, whose project include resolves
# PARTITION_CSV_PATH before this file selects the capacity-specific layout.
if(CONFIG_PARTITION_TABLE_CUSTOM AND CONFIG_PARTITION_TABLE_CUSTOM_FILENAME STREQUAL "partitions.csv")
    idf_build_get_property(project_dir PROJECT_DIR)
    if(CONFIG_ESPTOOLPY_FLASHSIZE STREQUAL "1MB")
        set(PARTITION_CSV_PATH "${project_dir}/partitions_1mb.csv")
    elseif(CONFIG_ESPTOOLPY_FLASHSIZE STREQUAL "2MB")
        set(PARTITION_CSV_PATH "${project_dir}/partitions_2mb.csv")
    elseif(CONFIG_ESPTOOLPY_FLASHSIZE STREQUAL "4MB")
        set(PARTITION_CSV_PATH "${project_dir}/partitions_4mb.csv")
    elseif(CONFIG_ESPTOOLPY_FLASHSIZE STREQUAL "8MB")
        set(PARTITION_CSV_PATH "${project_dir}/partitions_8mb.csv")
    elseif(CONFIG_ESPTOOLPY_FLASHSIZE MATCHES "^(16|32|64|128)MB$")
        set(PARTITION_CSV_PATH "${project_dir}/partitions.csv")
    else()
        message(FATAL_ERROR "Unsupported Flash size: ${CONFIG_ESPTOOLPY_FLASHSIZE}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PARTITION_CSV_PATH}")
    message(STATUS "Demo partition table (${CONFIG_ESPTOOLPY_FLASHSIZE} Flash): ${PARTITION_CSV_PATH}")
endif()

idf_component_register(SRCS "sd_board.c"
    INCLUDE_DIRS "."
    REQUIRES esp_board_manager fatfs sdmmc esp_driver_gpio)

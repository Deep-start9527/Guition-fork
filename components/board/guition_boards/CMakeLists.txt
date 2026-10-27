# BMGR compiles the selected board's setup and touch support sources.
idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES esp_board_manager esp_lcd_touch esp_lcd esp_driver_spi esp_driver_i2c esp_driver_gpio fatfs sdmmc
)

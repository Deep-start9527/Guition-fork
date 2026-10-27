/* SPDX-License-Identifier: MIT */
#include "sdkconfig.h"
#include "esp_board_manager.h"
#include "esp_err.h"

void app_main(void)
{
    /* Obtain only display resources. Do not initialize every declared bus. */
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("display_lcd"));
#if CONFIG_GUITION_TOUCH_RESISTIVE
    const char *touch_name = "lcd_touch_resistive";
#elif CONFIG_GUITION_TOUCH_NONE
    const char *touch_name = NULL;
#else
    const char *touch_name = "lcd_touch";
#endif
    if (touch_name && esp_board_manager_check_name(touch_name)) {
        ESP_ERROR_CHECK(esp_board_manager_init_device_by_name(touch_name));
    }
    if (esp_board_manager_check_name("lcd_brightness")) {
        ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("lcd_brightness"));
    }
}

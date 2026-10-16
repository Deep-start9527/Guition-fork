#pragma once

#include "esp_lv_adapter.h"
#include "dev_display_lcd.h"

esp_err_t demo_display_rotation_configure(const dev_display_lcd_config_t *lcd,
                                          esp_lv_adapter_display_config_t *config);
esp_err_t demo_display_rotation_attach(lv_display_t *display);
void demo_display_rotation_touch(esp_lv_adapter_touch_config_t *config);
void demo_display_rotation_cleanup(void);

#pragma once

#include "dev_display_lcd.h"
#include "esp_lv_adapter.h"

esp_err_t demo_display_buffers_configure(const dev_display_lcd_config_t *lcd,
                                        esp_lv_adapter_display_config_t *config);

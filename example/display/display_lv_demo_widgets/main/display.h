#pragma once

#include "esp_err.h"

esp_err_t display_init(void);
bool lvgl_display_lock(int timeout_ms);
void lvgl_display_unlock();
/* Caller holds adapter lock. Only affects shared SD/touch SPI boards. */
void display_sd_touch_enable(bool enabled);

/* SPDX-License-Identifier: MIT */
#pragma once
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Retry resources retained after a failed resistive touch initialization. */
esp_err_t guition_touch_bmgr_recover(void);

typedef struct {
    int sclk, mosi, miso, cs;
    unsigned half_period_us;
} guition_soft_spi_config_t;
esp_err_t guition_soft_spi_new(const guition_soft_spi_config_t *pins,
                               esp_lcd_panel_io_handle_t *out);

#ifdef __cplusplus
}
#endif

// SPDX-License-Identifier: MIT
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_board_manager_includes.h"
#include "esp_board_entry.h"
#include "esp_board_manager.h"
#include "esp_lcd_axs15260.h"
#include "esp_lcd_touch_axs15260.h"
#include "lcd_init_commands.h"

#define TAG     "JC5454F328"

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                      const esp_lcd_panel_dev_config_t *config,
                                      esp_lcd_panel_handle_t *out)
{
    if (!config || !out) return ESP_ERR_INVALID_ARG;
    // BMGR stores the RGB configuration; the controller needs its own wrapper.
    dev_display_lcd_config_t *board = NULL;
    esp_err_t ret = esp_board_manager_get_device_config("display_lcd", (void **)&board);
    if (ret != ESP_OK) return ret;
    if (!board) return ESP_ERR_INVALID_STATE;
    axs15260_vendor_config_t vendor = {
        .rgb_config = &board->sub_cfg.rgb_3wire_spi.rgb_panel_config,
        .init_cmds = guition_lcd_init_cmds,
        .init_cmds_size = sizeof(guition_lcd_init_cmds) / sizeof(guition_lcd_init_cmds[0]),
    };
    esp_lcd_panel_dev_config_t panel = *config;
    panel.vendor_config = &vendor;
    return esp_lcd_new_panel_axs15260(io, &panel, out);
}

__attribute__((weak)) esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                                          const esp_lcd_touch_config_t *touch_dev_config,
                                                          esp_lcd_touch_handle_t *ret_touch)
{
    /* Board Manager has already created the I2C panel IO for this touch
     * device. Reuse it so the driver and Board Manager share exactly one
     * i2c_master_dev_handle_t. lcd_cmd = -1 in the driver selects the raw
     * receive-only AXS15260 report transaction. */
    esp_err_t ret = esp_lcd_touch_new_i2c_axs15260(io, touch_dev_config, ret_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create AXS15260 touch driver: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

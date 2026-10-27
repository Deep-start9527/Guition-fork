#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_board_manager.h"
#include "esp_board_device.h"
#include "esp_board_manager_includes.h"
#include "lvgl.h"
#include "display_color.h"

static const char *TAG = "Display color";

esp_err_t demo_display_color_configure(void)
{
    dev_display_lcd_config_t *lcd = NULL;
    esp_err_t ret = esp_board_manager_get_device_config("display_lcd", (void **)&lcd);
    if (ret != ESP_OK) return ret;
    if (!lcd) return ESP_ERR_INVALID_STATE;

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    if (strcmp(lcd->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        // Generated board configuration is read-only; BMGR owns this RAM copy.
        dev_display_lcd_config_t config = *lcd;
        config.bits_per_pixel = LV_COLOR_DEPTH;
#if LV_COLOR_DEPTH == 16
        config.frame_format = DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_LE;
        config.sub_cfg.dsi.dpi_config.in_color_format = LCD_COLOR_FMT_RGB565;
        config.sub_cfg.dsi.dpi_config.out_color_format = LCD_COLOR_FMT_RGB565;
#else
        config.frame_format = DEV_DISPLAY_LCD_FRAME_FORMAT_RGB888;
        config.sub_cfg.dsi.dpi_config.in_color_format = LCD_COLOR_FMT_RGB888;
        config.sub_cfg.dsi.dpi_config.out_color_format = LCD_COLOR_FMT_RGB888;
#endif
        return esp_board_device_override_config("display_lcd", &config, sizeof(config));
    }
#endif
    if (lcd->bits_per_pixel != LV_COLOR_DEPTH) {
        ESP_LOGE(TAG, "Selected %d-bit color does not match this board's %" PRIu32
                 "-bit LCD; select its native format in Demo display", LV_COLOR_DEPTH, lcd->bits_per_pixel);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

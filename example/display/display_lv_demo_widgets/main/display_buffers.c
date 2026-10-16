#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "display_buffers.h"

static const char *TAG = "Display buffers";

esp_err_t demo_display_buffers_configure(const dev_display_lcd_config_t *lcd,
                                        esp_lv_adapter_display_config_t *config)
{
    int available = -1;
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    if (strcmp(lcd->sub_type, "dsi") == 0) {
        available = lcd->sub_cfg.dsi.dpi_config.num_fbs;
        if (available == 0) available = 1;
    }
#endif
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT || CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_3WIRE_SPI_SUPPORT
    const esp_lcd_rgb_panel_config_t *rgb = NULL;
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT
    if (strcmp(lcd->sub_type, "rgb") == 0) rgb = &lcd->sub_cfg.rgb.panel_config;
#endif
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_3WIRE_SPI_SUPPORT
    if (strcmp(lcd->sub_type, "rgb_3wire_spi") == 0) {
        rgb = &lcd->sub_cfg.rgb_3wire_spi.rgb_panel_config;
    }
#endif
    if (rgb) {
        available = rgb->flags.no_fb ? 0 : rgb->flags.double_fb ? 2 :
                    rgb->num_fbs ? rgb->num_fbs : 1;
    }
#endif
    (void)lcd;
    if (available < 0) return ESP_OK;

    const bool single = available == 1;
#if CONFIG_DEMO_LCD_SINGLE_FRAME_BUFFER
    if (!single) {
        ESP_LOGW(TAG, "Ignoring single-buffer option: board provides %d frame buffers", available);
    }
#endif
    if (single) {
        if (config->profile.rotation != ESP_LV_ADAPTER_ROTATE_0) {
            ESP_LOGE(TAG, "Single panel buffer does not support software rotation; increase board num_fbs or select 0 degrees");
            return ESP_ERR_NOT_SUPPORTED;
        }
        config->tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
        config->profile.buffer_height = 20;
        config->profile.require_double_buffer = false;
        config->profile.use_psram = false;
    }
    const int required = esp_lv_adapter_get_required_frame_buffer_count(
        config->tear_avoid_mode, config->profile.rotation);
    if (available < required) {
        ESP_LOGE(TAG, "Board num_fbs=%d, display mode requires %d; update board_devices.yaml and regenerate with bmgr",
                 available, required);
        return ESP_ERR_INVALID_ARG;
    }
    if (single) ESP_LOGW(TAG, "Single panel buffer refresh enabled; tearing may occur");
    return ESP_OK;
}

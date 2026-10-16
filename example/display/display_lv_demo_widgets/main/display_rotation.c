#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "display_rotation.h"
#include "display_rotation_geometry.h"

#if CONFIG_DEMO_LCD_ROTATION_HARDWARE
static const char *TAG = "Rotation";
#endif
static int s_width, s_height;
static uint16_t *s_pixels;
static size_t s_pixel_capacity;

static esp_err_t rotated_draw(lv_display_t *display, esp_lcd_panel_handle_t panel,
                              int x1, int y1, int x2, int y2,
                              const void *color_map, void *user_ctx)
{
    (void)display;
    (void)user_ctx;
    const int angle = CONFIG_DEMO_LCD_ROTATION_DEGREES;
    const int width = x2 - x1, height = y2 - y1;
    if (width <= 0 || height <= 0 || (size_t)width * height > s_pixel_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    int ax, ay, bx, by;
    demo_rotation_to_physical(x1, y1, s_width, s_height, angle, &ax, &ay);
    demo_rotation_to_physical(x2 - 1, y2 - 1, s_width, s_height, angle, &bx, &by);
    const int left = ax < bx ? ax : bx, top = ay < by ? ay : by;
    const int right = (ax > bx ? ax : bx) + 1, bottom = (ay > by ? ay : by) + 1;
    if (left < 0 || top < 0 || right > s_width || bottom > s_height) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint16_t *source = color_map;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int px, py;
            demo_rotation_to_physical(x1 + x, y1 + y, s_width, s_height, angle, &px, &py);
            s_pixels[(py - top) * (right - left) + px - left] = source[y * width + x];
        }
    }
    // LVGL waits for the previous flush before invoking another draw callback,
    // including with two draw buffers. Keep this DMA strip until IO completes.
    return esp_lcd_panel_draw_bitmap(panel, left, top, right, bottom, s_pixels);
}

static esp_err_t rotated_touch(esp_lcd_touch_handle_t touch,
                               esp_lcd_touch_point_data_t *points, uint8_t *count,
                               uint8_t max_count, void *user_ctx)
{
    (void)user_ctx;
    *count = 0;
    esp_err_t ret = esp_lcd_touch_read_data(touch);
    if (ret != ESP_OK) return ret;
    ret = esp_lcd_touch_get_data(touch, points, count, max_count);
    if (ret != ESP_OK) return ret;
    uint8_t valid = 0;
    for (uint8_t i = 0; i < *count; ++i) {
        // Driver coordinates already include the board's calibration flags.
        if (points[i].x >= s_width || points[i].y >= s_height) continue;
        int x, y;
        demo_rotation_to_logical(points[i].x, points[i].y, s_width, s_height,
                                 CONFIG_DEMO_LCD_ROTATION_DEGREES, &x, &y);
        points[valid] = points[i];
        points[valid].x = x;
        points[valid].y = y;
        ++valid;
    }
    *count = valid;
    return ESP_OK;
}

esp_err_t demo_display_rotation_configure(const dev_display_lcd_config_t *lcd,
                                          esp_lv_adapter_display_config_t *config)
{
    const int angle = CONFIG_DEMO_LCD_ROTATION_DEGREES;
    s_width = lcd->lcd_width;
    s_height = lcd->lcd_height;
    config->profile.rotation = angle;
    const bool spi = strcmp(lcd->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_SPI) == 0;
#if CONFIG_DEMO_SPI_DRAW_BUFFER_LINES
    if (spi && config->tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE) {
        config->profile.buffer_height = CONFIG_DEMO_SPI_DRAW_BUFFER_LINES;
        config->profile.use_psram = false;
#if CONFIG_DEMO_SPI_DOUBLE_DRAW_BUFFER
        config->profile.require_double_buffer = true;
#else
        config->profile.require_double_buffer = false;
#endif
    }
#endif
    if (angle == 0) return ESP_OK;
#if CONFIG_DEMO_LCD_ROTATION_HARDWARE
    if (angle == 180 && (spi || strcmp(lcd->sub_type, "dsi") == 0)) {
        // A half turn only toggles both mirror axes. Do not call swap_xy:
        // some MIPI controllers support mirroring but cannot swap axes.
        esp_err_t ret = esp_lcd_panel_mirror(config->panel, !lcd->mirror_x, !lcd->mirror_y);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LCD hardware 180-degree mirroring failed: %s", esp_err_to_name(ret));
            return ret;
        }
        config->profile.rotation = ESP_LV_ADAPTER_ROTATE_0;
        return ESP_OK;
    }
    if (!spi || s_width > 360 || s_height > 360) {
        ESP_LOGE(TAG, "Hardware 90/270-degree rotation requires a supported SPI LCD up to 360x360");
        return ESP_ERR_NOT_SUPPORTED;
    }
    bool swap, mirror_x, mirror_y;
    demo_rotation_panel_flags(lcd->swap_xy, lcd->mirror_x, lcd->mirror_y, angle,
                              &swap, &mirror_x, &mirror_y);
    esp_err_t ret = esp_lcd_panel_swap_xy(config->panel, swap);
    if (ret != ESP_OK) return ret;
    ret = esp_lcd_panel_mirror(config->panel, mirror_x, mirror_y);
    if (ret != ESP_OK) return ret;
#else
    if (!spi) {
#if CONFIG_DEMO_LCD_REFRESH_DOUBLE_FULL
        config->tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL;
#elif CONFIG_DEMO_LCD_REFRESH_TRIPLE_FULL
        config->tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL;
#elif CONFIG_DEMO_LCD_REFRESH_TRIPLE_PARTIAL
        config->tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
#else
        config->tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_PARTIAL;
#endif
        // Full-refresh modes use full panel buffers; partial modes use strips.
        config->profile.buffer_height = 20;
        config->profile.use_psram = false;
        return ESP_OK;
    }
    if (config->tear_avoid_mode == ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC &&
        config->te_sync.pipeline == ESP_LV_ADAPTER_TE_PIPELINE_ASYNC_FULL) {
        return ESP_OK;
    }
    if (config->tear_avoid_mode != ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE || LV_COLOR_DEPTH != 16) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif
    // Hardware-oriented SPI and the software SPI callback both consume logical
    // rectangles directly, so the adapter itself must not rotate them again.
    config->profile.rotation = ESP_LV_ADAPTER_ROTATE_0;
    if (angle == 90 || angle == 270) {
        config->profile.hor_res = s_height;
        config->profile.ver_res = s_width;
    }
#if CONFIG_DEMO_LCD_ROTATION_SOFTWARE
    s_pixel_capacity = (size_t)config->profile.hor_res * config->profile.buffer_height;
    s_pixels = heap_caps_malloc(s_pixel_capacity * sizeof(*s_pixels), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_pixels) return ESP_ERR_NO_MEM;
#endif
    return ESP_OK;
}

esp_err_t demo_display_rotation_attach(lv_display_t *display)
{
    if (!s_pixels) return ESP_OK;
    const esp_lv_adapter_draw_bitmap_callbacks_t callbacks = { .custom_draw_bitmap = rotated_draw };
    return esp_lv_adapter_set_draw_bitmap_callbacks(display, &callbacks, NULL);
}

void demo_display_rotation_touch(esp_lv_adapter_touch_config_t *config)
{
    if (CONFIG_DEMO_LCD_ROTATION_DEGREES != 0) {
        config->callbacks.custom_touch_read = rotated_touch;
    }
}

void demo_display_rotation_cleanup(void)
{
    free(s_pixels);
    s_pixels = NULL;
    s_pixel_capacity = 0;
}

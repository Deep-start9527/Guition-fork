/* SPDX-License-Identifier: MIT */
#include "sdkconfig.h"
#if CONFIG_GUITION_TOUCH_RESISTIVE
#include <stdlib.h>
#include "esp_log.h"
#include "guition_touch.h"
#include "esp_board_entry.h"
#include "esp_board_periph.h"
#include "dev_lcd_touch.h"
#include "periph_spi.h"
#include "esp_lcd_touch_xpt2046.h"
#include "gen_board_device_custom.h"

/* The first member is the handle interface shared with native BMGR touch. */
typedef struct {
    dev_lcd_touch_handles_t handles;
    const char *bus_name;
    bool bus_acquired;
} resistive_touch_t;

static resistive_touch_t *pending_cleanup;

static esp_err_t touch_deinit(void *handle)
{
    resistive_touch_t *device = handle;
    if (!device) return ESP_ERR_INVALID_ARG;
    esp_err_t ret;
    if (device->handles.touch_handle) {
        ret = esp_lcd_touch_del(device->handles.touch_handle);
        if (ret != ESP_OK) return ret;
        device->handles.touch_handle = NULL;
    }
    if (device->handles.io_handle) {
        ret = esp_lcd_panel_io_del(device->handles.io_handle);
        if (ret != ESP_OK) return ret;
        device->handles.io_handle = NULL;
    }
    if (device->bus_acquired) {
        ret = esp_board_periph_unref_handle(device->bus_name);
        if (ret != ESP_OK) return ret;
        device->bus_acquired = false;
    }
    free(device);
    return ESP_OK;
}

esp_err_t guition_touch_bmgr_recover(void)
{
    if (!pending_cleanup) return ESP_OK;
    esp_err_t ret = touch_deinit(pending_cleanup);
    if (ret == ESP_OK) pending_cleanup = NULL;
    return ret;
}

static esp_err_t touch_init(void *board_config, int config_size, void **out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    if (!board_config || config_size != sizeof(dev_custom_lcd_touch_resistive_config_t)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = guition_touch_bmgr_recover();
    if (ret != ESP_OK) return ret;
    const dev_custom_lcd_touch_resistive_config_t *profile = board_config;
    if (!profile->bus_name || !profile->bus_name[0] ||
        !profile->touch_config.x_max || !profile->touch_config.y_max ||
        profile->io_spi_config.cs_gpio_num < 0 || profile->io_spi_config.pclk_hz <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_lcd_panel_io_spi_config_t io_config = (esp_lcd_panel_io_spi_config_t) {
        .cs_gpio_num = profile->io_spi_config.cs_gpio_num,
        .dc_gpio_num = profile->io_spi_config.dc_gpio_num,
        .spi_mode = profile->io_spi_config.spi_mode,
        .pclk_hz = profile->io_spi_config.pclk_hz,
        .trans_queue_depth = profile->io_spi_config.trans_queue_depth,
        .lcd_cmd_bits = profile->io_spi_config.lcd_cmd_bits,
        .lcd_param_bits = profile->io_spi_config.lcd_param_bits,
        .cs_ena_pretrans = profile->io_spi_config.cs_ena_pretrans,
        .cs_ena_posttrans = profile->io_spi_config.cs_ena_posttrans,
        .flags = {
            .dc_high_on_cmd = profile->io_spi_config.flags.dc_high_on_cmd,
            .dc_low_on_data = profile->io_spi_config.flags.dc_low_on_data,
            .dc_low_on_param = profile->io_spi_config.flags.dc_low_on_param,
            .octal_mode = profile->io_spi_config.flags.octal_mode,
            .quad_mode = profile->io_spi_config.flags.quad_mode,
            .sio_mode = profile->io_spi_config.flags.sio_mode,
            .psram_dma_direct = profile->io_spi_config.flags.psram_dma_direct,
            .lsb_first = profile->io_spi_config.flags.lsb_first,
            .cs_high_active = profile->io_spi_config.flags.cs_high_active,
        },
    };
    const esp_lcd_touch_config_t touch_config = (esp_lcd_touch_config_t) {
        .x_max = profile->touch_config.x_max,
        .y_max = profile->touch_config.y_max,
        .rst_gpio_num = profile->touch_config.rst_gpio_num,
        .int_gpio_num = profile->touch_config.int_gpio_num,
        .levels = {
            .reset = profile->touch_config.levels.reset,
            .interrupt = profile->touch_config.levels.interrupt,
        },
        .flags = {
            .swap_xy = profile->touch_config.flags.swap_xy,
            .mirror_x = profile->touch_config.flags.mirror_x,
            .mirror_y = profile->touch_config.flags.mirror_y,
        },
    };

    resistive_touch_t *device = calloc(1, sizeof(*device));
    if (!device) return ESP_ERR_NO_MEM;
    device->bus_name = profile->bus_name;
    void *bus = NULL;
    ret = esp_board_periph_ref_handle(profile->bus_name, &bus);
    if (ret == ESP_OK) {
        device->bus_acquired = true;
        if (!bus) ret = ESP_ERR_INVALID_STATE;
        else ret = esp_lcd_new_panel_io_spi(((periph_spi_handle_t *)bus)->spi_port,
                                            &io_config, &device->handles.io_handle);
    }
    if (ret == ESP_OK && !device->handles.io_handle) ret = ESP_ERR_INVALID_STATE;
    if (ret == ESP_OK) {
        ret = esp_lcd_touch_new_spi_xpt2046(device->handles.io_handle,
                                           &touch_config, &device->handles.touch_handle);
        if (ret == ESP_OK && !device->handles.touch_handle) ret = ESP_ERR_INVALID_STATE;
    }
    if (ret != ESP_OK) {
        /* BMGR discards failed custom init outputs; retain failed cleanup. */
        esp_err_t cleanup = touch_deinit(device);
        if (cleanup != ESP_OK) {
            pending_cleanup = device;
            ESP_LOGE("guition_touch", "Touch init failed (%s), cleanup needs retry (%s)",
                     esp_err_to_name(ret), esp_err_to_name(cleanup));
        }
        return ret;
    }
    *out = device;
    return ESP_OK;
}

CUSTOM_DEVICE_IMPLEMENT(lcd_touch_resistive, touch_init, touch_deinit);
#endif

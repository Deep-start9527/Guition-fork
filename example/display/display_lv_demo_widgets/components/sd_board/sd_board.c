/* SPDX-License-Identifier: MIT */
#include "sdkconfig.h"
#include "sd_board.h"

#if CONFIG_GUITION_SD_ENABLED
#include <string.h>
#include "esp_board_manager.h"
#include "dev_fs_fat.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "SD_Board";
static const dev_fs_fat_config_t *config;
static const sdmmc_card_t *card;
static bool device_owned;

static esp_err_t load_config(void)
{
    if (config) return ESP_OK;
    esp_err_t ret = esp_board_manager_get_device_config("fs_sdcard", (void **)&config);
    return ret == ESP_OK && !config ? ESP_ERR_INVALID_STATE : ret;
}

static esp_err_t deselect_sd(void)
{
    esp_err_t ret = load_config();
    if (ret != ESP_OK) return ret;
    if (!config->sub_type || strcmp(config->sub_type, "spi")) return ESP_OK;
    /* SDSPI teardown leaves CS as an input; keep SD inactive on a shared bus. */
    ret = gpio_set_level(config->sub_cfg.spi.cs_gpio_num, 1);
    if (ret != ESP_OK) return ret;
    return gpio_set_direction(config->sub_cfg.spi.cs_gpio_num, GPIO_MODE_OUTPUT);
}

bool sd_board_supported(void) { return true; }
const sdmmc_card_t *sd_board_card(void) { return card; }

esp_err_t sd_board_unmount(void)
{
    if (device_owned) {
        esp_err_t ret = esp_board_manager_deinit_device_by_name("fs_sdcard");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Board Manager SD deinit failed: %s", esp_err_to_name(ret));
            return ret;
        }
        device_owned = false;
        card = NULL;
    }
    return deselect_sd();
}

esp_err_t sd_board_mount(void)
{
    if (card) return ESP_OK;
    /* A failed handle lookup can leave a reference whose release must retry. */
    if (device_owned) {
        esp_err_t ret = sd_board_unmount();
        if (ret != ESP_OK) return ret;
    }
    esp_err_t ret = load_config();
    if (ret != ESP_OK) return ret;
    ret = esp_board_manager_init_device_by_name("fs_sdcard");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Board Manager SD init failed: %s; see SD driver logs", esp_err_to_name(ret));
        esp_err_t cleanup = deselect_sd();
        return cleanup == ESP_OK ? ret : cleanup;
    }
    device_owned = true;
    dev_fs_fat_handle_t *handle = NULL;
    ret = esp_board_manager_get_device_handle("fs_sdcard", (void **)&handle);
    if (ret == ESP_OK && handle && handle->card) {
        card = handle->card;
        return ESP_OK;
    }
    if (ret == ESP_OK) ret = ESP_ERR_INVALID_STATE;
    ESP_LOGE(TAG, "Board Manager SD handle unavailable: %s", esp_err_to_name(ret));
    esp_err_t cleanup = sd_board_unmount();
    return cleanup == ESP_OK ? ret : cleanup;
}

esp_err_t sd_board_before_display(void)
{
#if CONFIG_ESP_BOARD_ESP32_4848S040
    /* LCD control shares SD pins; enter SPI mode, then release the bus. */
    esp_err_t ret = gpio_set_level(39, 1);
    if (ret != ESP_OK) return ret;
    ret = gpio_set_direction(39, GPIO_MODE_OUTPUT);
    if (ret != ESP_OK) return ret;
    ret = sd_board_mount();
    if (ret != ESP_OK) return ret;
    return sd_board_unmount();
#else
#if CONFIG_ESP_BOARD_JC4827W543 || CONFIG_ESP_BOARD_ESP32_4827S043
    static bool touch_deselected;
    if (!touch_deselected) {
        /* Only before touch initialization: direction changes detach SPI CS. */
        esp_err_t ret = gpio_set_level(38, 1);
        if (ret != ESP_OK) return ret;
        ret = gpio_set_direction(38, GPIO_MODE_OUTPUT);
        if (ret != ESP_OK) return ret;
        touch_deselected = true;
    }
#endif
    return sd_board_mount();
#endif
}
#else
bool sd_board_supported(void) { return false; }
const sdmmc_card_t *sd_board_card(void) { return NULL; }
esp_err_t sd_board_mount(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t sd_board_unmount(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t sd_board_before_display(void) { return ESP_ERR_NOT_SUPPORTED; }
#endif

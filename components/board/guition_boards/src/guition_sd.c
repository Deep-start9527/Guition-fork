/* SPDX-License-Identifier: MIT */
#include "sdkconfig.h"
#include "guition_sd.h"
#if CONFIG_GUITION_SD_ENABLED
#include <string.h>
#include "soc/soc_caps.h"
#include "esp_board_manager.h"
#include "esp_board_periph.h"
#include "dev_fs_fat.h"
#if CONFIG_ESP_BOARD_PERIPH_SPI_SUPPORT
#include "periph_spi.h"
#endif
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#if SOC_SDMMC_HOST_SUPPORTED
#include "driver/sdmmc_host.h"
#endif
#if SOC_GP_LDO_SUPPORTED
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

static sdmmc_card_t *card;
static const dev_fs_fat_config_t *cfg;
static bool bus_ref;
#if CONFIG_ESP_BOARD_ESP32_4848S040
static sdspi_dev_handle_t probe_device;
static bool probe_attached;
#endif
#if SOC_GP_LDO_SUPPORTED
static sd_pwr_ctrl_handle_t power;
#endif

static esp_err_t get_config(void)
{
    if (cfg) return ESP_OK;
    return esp_board_manager_get_device_config("fs_sdcard", (void **)&cfg);
}
static esp_err_t release_resources(void)
{
    esp_err_t ret;
#if CONFIG_ESP_BOARD_ESP32_4848S040
    if (probe_attached) {
        ret = sdspi_host_remove_device(probe_device);
        if (ret != ESP_OK) return ret;
        probe_attached = false;
    }
#endif
    if (bus_ref) {
        // IDF's SDSPI teardown changes CS to input. Keep the card deselected
        // while another device (S040 LCD control or shared touch) clocks SPI.
        gpio_set_level(cfg->sub_cfg.spi.cs_gpio_num, 1);
        ret = gpio_set_direction(cfg->sub_cfg.spi.cs_gpio_num, GPIO_MODE_OUTPUT);
        if (ret != ESP_OK) return ret;
        ret = esp_board_periph_unref_handle(cfg->sub_cfg.spi.spi_bus_name);
        if (ret != ESP_OK) return ret;
        bus_ref = false;
    }
#if SOC_GP_LDO_SUPPORTED
    if (power) {
        ret = sd_pwr_ctrl_del_on_chip_ldo(power);
        if (ret != ESP_OK) return ret;
        power = NULL;
    }
#endif
    return ESP_OK;
}
#if CONFIG_ESP_BOARD_PERIPH_SPI_SUPPORT
static esp_err_t inactive_cs(void)
{
    int cs = -1;
#if CONFIG_ESP_BOARD_ESP32_4827S043 || CONFIG_ESP_BOARD_JC4827W543
    cs = 38;
#elif CONFIG_ESP_BOARD_ESP32_4848S040
    cs = 39;
#endif
    if (cs < 0) return ESP_OK;
    /* Set output latch before enabling output; do not glitch another device. */
    gpio_set_level(cs, 1);
    return gpio_set_direction(cs, GPIO_MODE_OUTPUT);
}
#endif
bool guition_sd_supported(void) { return true; }
const sdmmc_card_t *guition_sd_card(void) { return card; }

esp_err_t guition_sd_unmount(void)
{
    if (card) {
        esp_err_t ret = esp_vfs_fat_sdcard_unmount("/sdcard", card);
        if (ret != ESP_OK) return ret;
        card = NULL;
    }
    return release_resources();
}

esp_err_t guition_sd_mount(void)
{
    if (card) return ESP_OK;
    esp_err_t ret = get_config();
    if (ret != ESP_OK || !cfg) return ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret;
    ret = release_resources();
    if (ret != ESP_OK) return ret;
    esp_vfs_fat_mount_config_t mount = { .format_if_mount_failed = false, .max_files = 5,
        .allocation_unit_size = 16384 };
    if (!strcmp(cfg->sub_type, "spi")) {
#if CONFIG_ESP_BOARD_PERIPH_SPI_SUPPORT
        ret = inactive_cs();
        if (ret != ESP_OK) return ret;
        periph_spi_handle_t *bus = NULL;
        ret = esp_board_periph_ref_handle(cfg->sub_cfg.spi.spi_bus_name, (void **)&bus);
        if (ret != ESP_OK) return ret;
        bus_ref = true;
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = bus->spi_port;
        host.max_freq_khz = 20000;
        host.command_timeout_ms = 1000;
        sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot.host_id = bus->spi_port;
        slot.gpio_cs = cfg->sub_cfg.spi.cs_gpio_num;
        ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mount, &card);
#else
        ret = ESP_ERR_NOT_SUPPORTED;
#endif
    } else {
#if SOC_SDMMC_HOST_SUPPORTED
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = cfg->sub_cfg.sdmmc.slot;
        host.max_freq_khz = 20000;
        host.command_timeout_ms = 1000;
#if SOC_GP_LDO_SUPPORTED
        sd_pwr_ctrl_ldo_config_t ldo = { .ldo_chan_id = cfg->sub_cfg.sdmmc.ldo_chan_id };
        ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &power);
        if (ret != ESP_OK) return ret;
        host.pwr_ctrl_handle = power;
#endif
        const dev_fs_fat_sdmmc_pins_t *p = &cfg->sub_cfg.sdmmc.pins;
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 4;
        slot.clk = p->clk; slot.cmd = p->cmd;
        slot.d0 = p->d0; slot.d1 = p->d1; slot.d2 = p->d2; slot.d3 = p->d3;
        slot.cd = -1; slot.wp = -1;
        ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount, &card);
#else
        ret = ESP_ERR_NOT_SUPPORTED;
#endif
    }
    if (ret != ESP_OK) {
        card = NULL;
        /* A failed release stays recorded and is retried before the next mount. */
        esp_err_t cleanup = release_resources();
        if (cleanup != ESP_OK) return cleanup;
    }
    return ret;
}
esp_err_t guition_sd_before_display(void)
{
#if CONFIG_ESP_BOARD_ESP32_4848S040
    if (card) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = get_config();
    if (ret != ESP_OK) return ret;
    ret = release_resources();
    if (ret != ESP_OK) return ret;
    ret = inactive_cs();
    if (ret != ESP_OK) return ret;
    periph_spi_handle_t *bus = NULL;
    ret = esp_board_periph_ref_handle(cfg->sub_cfg.spi.spi_bus_name, (void **)&bus);
    if (ret != ESP_OK) return ret;
    bus_ref = true;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 20000;
    host.command_timeout_ms = 1000;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = bus->spi_port;
    slot.gpio_cs = cfg->sub_cfg.spi.cs_gpio_num;
    ret = sdspi_host_init();
    if (ret == ESP_OK) {
        ret = sdspi_host_init_device(&slot, &probe_device);
        if (ret == ESP_OK) {
            probe_attached = true;
            host.slot = probe_device;
            sdmmc_card_t probe = {0};
            ret = sdmmc_card_init(&host, &probe);
        }
    }
    esp_err_t cleanup = release_resources();
    return cleanup == ESP_OK ? ret : cleanup;
#else
    /* Shared XPT2046 stays inactive until the card has entered SPI mode. */
    return guition_sd_mount();
#endif
}
#else
bool guition_sd_supported(void) { return false; }
const sdmmc_card_t *guition_sd_card(void) { return NULL; }
esp_err_t guition_sd_mount(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t guition_sd_unmount(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t guition_sd_before_display(void) { return ESP_ERR_NOT_SUPPORTED; }
#endif

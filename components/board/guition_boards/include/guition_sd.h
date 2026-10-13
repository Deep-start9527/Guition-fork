/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "sdmmc_cmd.h"
/* Lifecycle calls are serialized by the application. While remounting a shared
 * SPI card, hold the LVGL adapter lock to pause its touch polling. Do not call
 * BMGR init(fs_sdcard) as well: this adapter owns that configuration's resources. */
bool guition_sd_supported(void);
esp_err_t guition_sd_mount(void);
esp_err_t guition_sd_unmount(void);
const sdmmc_card_t *guition_sd_card(void);
/* Call before LCD/touch initialization. S040 probes SPI mode then releases the
 * temporary bus. Call unmount before reinitializing its LCD; never reset/power
 * cycle the card between this probe and LCD initialization. */
esp_err_t guition_sd_before_display(void);

/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "sdmmc_cmd.h"

bool sd_board_supported(void);
const sdmmc_card_t *sd_board_card(void);
esp_err_t sd_board_mount(void);
esp_err_t sd_board_unmount(void);
esp_err_t sd_board_before_display(void);

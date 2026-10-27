#pragma once

#include "esp_err.h"

/* Apply the display format before Board Manager initializes the LCD. */
esp_err_t demo_display_color_configure(void);

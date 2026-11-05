#pragma once

#include "lvgl.h"

typedef void (*app_gpio_toast_cb_t)(const char *message);

/** Build digital I/O and single-channel PWM controls for the board header. */
void app_gpio_view_build(lv_obj_t *parent, app_gpio_toast_cb_t toast_cb);
/** Stop polling and release the header pins to floating inputs. */
void app_gpio_view_cleanup(void);

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#include "lvgl.h"

#if CONFIG_DEMO_DISPLAY_PERF_STATS
/* Call once per display while holding the LVGL lock. Display deletion cleans up.
 * Measures LVGL refresh processing, including synchronous flush waits, but not
 * asynchronous scanout or physical presented FPS. flush_avg counts the union
 * of synchronous flush callbacks and flush waits; render_other_avg is the
 * remaining refresh processing (layout, drawing and other work). */
esp_err_t demo_display_perf_attach(lv_display_t *display);
#else
static inline esp_err_t demo_display_perf_attach(lv_display_t *display)
{
    (void)display;
    return ESP_OK;
}
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_board_manager.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "display.h"

void app_main(void)
{
    esp_err_t ret = display_init();
    if (ret != ESP_OK) {
        ESP_LOGE("Demo", "Failed to initialize display: %s", esp_err_to_name(ret));
        return;
    }

    /* Create GUI and wire up */
    if (lvgl_display_lock(1000)) {
        lv_demo_widgets();
        // lv_demo_benchmark();

        lvgl_display_unlock();
    }
}

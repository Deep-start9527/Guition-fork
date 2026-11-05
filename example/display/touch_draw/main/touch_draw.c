#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "lvgl.h"
#include "display.h"

static lv_obj_t *canvas;
static lv_obj_t *coordinates;
static lv_obj_t *clear_button;
static lv_draw_buf_t *pixels;

/* One slot per virtual pointer; matches the menu's maximum point count. */
static struct {
    lv_indev_t *indev;
    lv_point_t point;
    bool active;
} touch_points[5];

static void update_coordinates(lv_indev_t *indev, bool pressed)
{
    size_t slot = 0;
    for (; slot < 5; ++slot) {
        if (touch_points[slot].indev == indev) break;
    }
    if (slot == 5 && pressed) {
        for (slot = 0; slot < 5; ++slot) {
            if (!touch_points[slot].indev) {
                touch_points[slot].indev = indev;
                break;
            }
        }
    }
    if (slot == 5) return;
    touch_points[slot].active = pressed;
    if (pressed) lv_indev_get_point(indev, &touch_points[slot].point);

    char text[256];
    size_t used = 0;
    text[0] = '\0';
    for (size_t i = 0; i < 5; ++i) {
        if (!touch_points[i].active) continue;
        int written = snprintf(text + used, sizeof(text) - used,
                               "%sP%u X: %ld  Y: %ld", used ? "\n" : "",
                               (unsigned)i + 1, (long)touch_points[i].point.x,
                               (long)touch_points[i].point.y);
        if (written < 0 || (size_t)written >= sizeof(text) - used) break;
        used += (size_t)written;
    }
    lv_label_set_text(coordinates, text);
}

static void clear_drawing(void)
{
    /* Indexed pixels follow the palette; index zero is white. */
    memset(lv_draw_buf_goto_xy(pixels, 0, 0), 0,
           (size_t)pixels->header.stride * pixels->header.h);
    lv_obj_invalidate(canvas);
}

static void touch_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (code == LV_EVENT_CLICKED && target == clear_button) {
        clear_drawing();
        return;
    }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
        code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) return;
    bool pressed = code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING;
    update_coordinates(indev, pressed);
    if (!pressed) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    if (target != canvas) return;

    lv_area_t button_area;
    lv_obj_get_coords(clear_button, &button_area);
    /* A canvas-owned drag must not paint underneath the clear button. */
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            int x = point.x + dx;
            int y = point.y + dy;
            if (dx * dx + dy * dy > 9 || x < 0 || y < 0 ||
                x >= (int)pixels->header.w || y >= (int)pixels->header.h) continue;
            if (x >= button_area.x1 && x <= button_area.x2 &&
                y >= button_area.y1 && y <= button_area.y2) continue;
            /* The blue channel carries the palette index for indexed canvases. */
            lv_canvas_set_px(canvas, x, y, lv_color_make(0, 0, 1), LV_OPA_COVER);
        }
    }
}

static esp_err_t create_drawing(void)
{
    lv_obj_t *screen = lv_screen_active();
    int32_t width = lv_display_get_horizontal_resolution(NULL);
    int32_t height = lv_display_get_vertical_resolution(NULL);
    if (width < 120 || height < 100) return ESP_ERR_NOT_SUPPORTED;
    pixels = lv_draw_buf_create(width, height, LV_COLOR_FORMAT_I1, 0);
    if (!pixels) return ESP_ERR_NO_MEM;
    memset(touch_points, 0, sizeof(touch_points));

    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(screen, false);
    canvas = lv_canvas_create(screen);
    lv_canvas_set_draw_buf(canvas, pixels);
    lv_canvas_set_palette(canvas, 0, lv_color_to_32(lv_color_white(), LV_OPA_COVER));
    lv_canvas_set_palette(canvas, 1, lv_color_to_32(lv_color_black(), LV_OPA_COVER));
    lv_obj_set_pos(canvas, 0, 0);
    lv_obj_set_clickable(canvas, true);
    lv_obj_set_scrollable(canvas, false);
    clear_drawing();

    clear_button = lv_button_create(screen);
    lv_obj_remove_style_all(clear_button);
    lv_obj_set_size(clear_button, 80, 40);
    lv_obj_align(clear_button, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_bg_color(clear_button, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(clear_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(clear_button, lv_color_black(), 0);
    lv_obj_set_style_border_width(clear_button, 1, 0);
    lv_obj_set_style_text_color(clear_button, lv_color_black(), 0);
    lv_obj_t *label = lv_label_create(clear_button);
    lv_label_set_text(label, "Clear");
    lv_obj_center(label);

    coordinates = lv_label_create(screen);
    lv_obj_set_size(coordinates, width * 3 / 5, LV_SIZE_CONTENT);
    lv_obj_align(coordinates, LV_ALIGN_BOTTOM_MID, 0, -height / 8);
    lv_obj_set_style_text_color(coordinates, lv_color_black(), 0);
    lv_obj_set_style_text_align(coordinates, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(coordinates, 5, 0);
    lv_label_set_text(coordinates, "");
    lv_obj_set_style_bg_opa(coordinates, LV_OPA_TRANSP, 0);
    lv_obj_set_clickable(coordinates, false);
    lv_obj_add_event_cb(canvas, touch_event, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(clear_button, touch_event, LV_EVENT_ALL, NULL);
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret = display_init();
    if (ret == ESP_OK) {
        if (!lvgl_display_lock(1000)) {
            ESP_LOGE("touch_draw", "Display lock timed out");
            return;
        }
        ret = create_drawing();
        lvgl_display_unlock();
    }
    if (ret != ESP_OK) {
        ESP_LOGE("touch_draw", "Initialization failed: %s", esp_err_to_name(ret));
    }
}

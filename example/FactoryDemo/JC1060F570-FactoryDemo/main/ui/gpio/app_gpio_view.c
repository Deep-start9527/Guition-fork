#include "app_gpio_view.h"

#include <stdlib.h>

#include "board_capabilities.h"
#include "desktop_fullscreen_layout.h"
#include "desktop_font.h"
#include "desktop_language.h"
#include "gpio_control.h"

#define L(key) desktop_language_lookup(key)
#define PIN_COUNT 10

static const int s_pins[PIN_COUNT] = {8, 9, 10, 11, 12, 13, 14, 15, 18, 19};
static lv_obj_t *s_modes[PIN_COUNT];
static lv_obj_t *s_levels[PIN_COUNT];
static lv_obj_t *s_outputs[PIN_COUNT];
static lv_obj_t *s_pin, *s_frequency, *s_duty, *s_status, *s_pwm_status, *s_keyboard;
static lv_timer_t *s_timer;
static app_gpio_toast_cb_t s_toast;
static bool s_active;

static lv_obj_t *label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    return obj;
}

static lv_obj_t *dropdown(lv_obj_t *parent, const char *options)
{
    lv_obj_t *obj = lv_dropdown_create(parent);
    const lv_font_t *font = desktop_font_get_primary();
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
    lv_obj_set_style_text_font(obj, lv_font_get_default(), LV_PART_INDICATOR);
    lv_dropdown_set_options(obj, options);
    /* The popup is reparented to the screen when opened, outside the page's font inheritance. */
    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_set_style_text_font(list, font, LV_PART_MAIN);
    lv_obj_set_style_text_font(list, font, LV_PART_SELECTED);
    return obj;
}

static void report(esp_err_t err)
{
    if (err == ESP_OK) {
        lv_label_set_text(s_status, "");
        return;
    }
    lv_label_set_text_fmt(s_status, "%s: %s", L(DESKTOP_TEXT_GPIO_ERROR), esp_err_to_name(err));
    if (s_toast) s_toast(lv_label_get_text(s_status));
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    bool pwm = false;
    bool unknown = false;
    for (int i = 0; i < PIN_COUNT; ++i) {
        gpio_control_state_t state;
        if (gpio_control_get_state(s_pins[i], &state) != ESP_OK) {
            unknown = true;
            lv_label_set_text(s_levels[i], "?");
            lv_dropdown_set_selected(s_modes[i], 5);
            lv_obj_remove_state(s_outputs[i], LV_STATE_CHECKED);
            lv_obj_add_state(s_outputs[i], LV_STATE_DISABLED);
            continue;
        }
        lv_dropdown_set_selected(s_modes[i], (uint32_t)state.mode);
        lv_obj_remove_state(s_outputs[i], LV_STATE_CHECKED | LV_STATE_DISABLED);
        if (state.mode != GPIO_CONTROL_OUTPUT) lv_obj_add_state(s_outputs[i], LV_STATE_DISABLED);
        if (state.level) lv_obj_add_state(s_outputs[i], LV_STATE_CHECKED);
        int level;
        if (state.mode == GPIO_CONTROL_PWM) {
            pwm = true;
            lv_label_set_text(s_levels[i], "PWM");
            lv_label_set_text_fmt(s_pwm_status, "GPIO%d · %lu Hz · %u%%", s_pins[i],
                                  (unsigned long)state.frequency_hz, state.duty_percent);
        } else if (gpio_control_read(s_pins[i], &level) == ESP_OK) {
            lv_label_set_text(s_levels[i], level ? "1" : "0");
        } else {
            lv_label_set_text(s_levels[i], "?");
        }
    }
    if (!pwm) lv_label_set_text(s_pwm_status, unknown ? L(DESKTOP_TEXT_GPIO_ERROR) : L(DESKTOP_TEXT_GPIO_PWM_STOPPED));
}

static void mode_cb(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    uint32_t mode = lv_dropdown_get_selected(s_modes[index]);
    if (mode == GPIO_CONTROL_PWM) {
        if (s_toast) s_toast(L(DESKTOP_TEXT_GPIO_PWM_HINT));
    } else if (mode <= GPIO_CONTROL_OUTPUT) {
        report(gpio_control_set_mode(s_pins[index], (gpio_control_mode_t)mode));
    }
    refresh(NULL);
}

static void output_cb(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    report(gpio_control_write(s_pins[index], lv_obj_has_state(s_outputs[index], LV_STATE_CHECKED)));
    refresh(NULL);
}

static void pwm_cb(lv_event_t *event)
{
    (void)event;
    const char *frequency = lv_textarea_get_text(s_frequency);
    const char *duty = lv_textarea_get_text(s_duty);
    unsigned long hz = strtoul(frequency, NULL, 10);
    unsigned long percent = strtoul(duty, NULL, 10);
    if (!*frequency || !*duty || hz < 1 || hz > 20000 || percent > 100) {
        report(ESP_ERR_INVALID_ARG);
        return;
    }
    int index = (int)lv_dropdown_get_selected(s_pin);
    report(gpio_control_pwm(s_pins[index], (uint32_t)hz, (uint8_t)percent));
    refresh(NULL);
}

static void stop_cb(lv_event_t *event)
{
    (void)event;
    for (int i = 0; i < PIN_COUNT; ++i) {
        gpio_control_state_t state;
        if (gpio_control_get_state(s_pins[i], &state) != ESP_OK || state.mode == GPIO_CONTROL_PWM) {
            report(gpio_control_set_mode(s_pins[i], GPIO_CONTROL_INPUT));
        }
    }
    refresh(NULL);
}

static void keyboard_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(s_keyboard, NULL);
        lv_obj_set_hidden(s_keyboard, true);
    }
}

static void field_cb(lv_event_t *event)
{
    lv_keyboard_set_textarea(s_keyboard, lv_event_get_target_obj(event));
    lv_obj_set_hidden(s_keyboard, false);
    lv_obj_move_foreground(s_keyboard);
}

static lv_obj_t *field(lv_obj_t *parent, const char *title, const char *value, uint32_t length)
{
    label(parent, title);
    lv_obj_t *obj = lv_textarea_create(parent);
    lv_obj_set_width(obj, LV_PCT(100));
    lv_textarea_set_one_line(obj, true);
    lv_textarea_set_accepted_chars(obj, "0123456789");
    lv_textarea_set_max_length(obj, length);
    lv_textarea_set_text(obj, value);
    lv_obj_add_event_cb(obj, field_cb, LV_EVENT_CLICKED, NULL);
    return obj;
}

static void button(lv_obj_t *parent, const char *text, lv_event_cb_t callback)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_height(obj, 38);
    lv_obj_center(label(obj, text));
    lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, NULL);
}

void app_gpio_view_cleanup(void)
{
    if (s_timer) lv_timer_delete(s_timer);
    s_timer = NULL;
    if (s_active) {
        esp_err_t err = gpio_control_cleanup();
        if (err != ESP_OK && s_toast) s_toast(esp_err_to_name(err));
        s_active = err != ESP_OK;
    }
    s_toast = NULL;
    s_keyboard = NULL;
}

void app_gpio_view_build(lv_obj_t *parent, app_gpio_toast_cb_t toast_cb)
{
    app_gpio_view_cleanup();
    s_toast = toast_cb;
    const desktop_fullscreen_layout_colors_t colors = {
        .canvas = lv_color_hex(0xDCE8F5), .surface = lv_color_hex(0xF7FAFD),
        .ink = lv_color_hex(0x122033), .ink_muted = lv_color_hex(0x53667B),
    };
    desktop_fullscreen_layout_t layout;
    if (!desktop_fullscreen_layout_create(parent, &colors, "", "", "", &layout)) return;
    desktop_fullscreen_layout_remove_page_header(&layout);
    lv_obj_set_style_text_font(parent, desktop_font_get_primary(), LV_PART_MAIN);
    if (!FACTORY_BOARD_HEADER_GPIO) {
        label(layout.content, L(DESKTOP_TEXT_GPIO_UNSUPPORTED));
        return;
    }
    s_active = true;
    esp_err_t err = gpio_control_init();
    if (err != ESP_OK) {
        lv_obj_t *message = label(layout.content, L(DESKTOP_TEXT_GPIO_ERROR));
        lv_label_set_text_fmt(message, "%s: %s", L(DESKTOP_TEXT_GPIO_ERROR), esp_err_to_name(err));
        return;
    }
    lv_obj_t *pins = desktop_fullscreen_layout_create_panel(
        layout.content, colors.surface, 61, 12, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(pins, true);
    lv_obj_set_style_pad_all(pins, 10, 0);
    lv_obj_set_style_pad_row(pins, 8, 0);
    label(pins, L(DESKTOP_TEXT_GPIO_HEADER));
    for (int i = 0; i < PIN_COUNT; ++i) {
        lv_obj_t *row = lv_obj_create(pins);
        lv_obj_set_size(row, LV_PCT(100), 54);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollable(row, false);
        lv_obj_t *name = label(row, "");
        lv_label_set_text_fmt(name, "GPIO%d", s_pins[i]);
        s_modes[i] = dropdown(row, L(DESKTOP_TEXT_GPIO_MODES));
        lv_obj_set_width(s_modes[i], LV_PCT(43));
        lv_obj_add_event_cb(s_modes[i], mode_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
        s_levels[i] = label(row, "0");
        s_outputs[i] = lv_switch_create(row);
        lv_obj_set_size(s_outputs[i], 42, 24);
        lv_obj_add_event_cb(s_outputs[i], output_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
    }
    lv_obj_t *pwm = desktop_fullscreen_layout_create_panel(
        layout.content, colors.surface, 38, 12, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(pwm, true);
    lv_obj_set_style_pad_all(pwm, 12, 0);
    lv_obj_set_style_pad_row(pwm, 8, 0);
    label(pwm, L(DESKTOP_TEXT_GPIO_PWM));
    s_pin = dropdown(pwm, "GPIO8\nGPIO9\nGPIO10\nGPIO11\nGPIO12\nGPIO13\nGPIO14\nGPIO15\nGPIO18\nGPIO19");
    lv_obj_set_width(s_pin, LV_PCT(100));
    s_frequency = field(pwm, L(DESKTOP_TEXT_GPIO_FREQUENCY), "1000", 5);
    s_duty = field(pwm, L(DESKTOP_TEXT_GPIO_DUTY), "50", 3);
    button(pwm, L(DESKTOP_TEXT_GPIO_APPLY), pwm_cb);
    button(pwm, L(DESKTOP_TEXT_GPIO_STOP), stop_cb);
    s_pwm_status = label(pwm, "");
    lv_obj_set_width(s_pwm_status, LV_PCT(100));
    s_status = label(pwm, "");
    lv_obj_set_width(s_status, LV_PCT(100));
    s_keyboard = lv_keyboard_create(parent);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(s_keyboard, LV_PCT(92), LV_PCT(45));
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_hidden(s_keyboard, true);
    lv_obj_add_event_cb(s_keyboard, keyboard_cb, LV_EVENT_ALL, NULL);
    s_timer = lv_timer_create(refresh, 100, NULL);
    if (!s_timer) report(ESP_ERR_NO_MEM);
    refresh(NULL);
}

#include "gpio_control.h"

#include <stdbool.h>
#include <stddef.h>
#include "board_capabilities.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const int pins[] = {8, 9, 10, 11, 12, 13, 14, 15, 18, 19};
#define PIN_COUNT (sizeof(pins) / sizeof(pins[0]))
static gpio_control_state_t states[PIN_COUNT];
static bool owned[PIN_COUNT];
static bool known[PIN_COUNT];
static bool initialized;
static bool timer_owned;
static int pwm_index = -1;

static int pin_index(int pin)
{
    for (size_t i = 0; i < PIN_COUNT; ++i) {
        if (pins[i] == pin) return (int)i;
    }
    return -1;
}

static esp_err_t validate(int pin, int *index)
{
    if (!FACTORY_BOARD_HEADER_GPIO) return ESP_ERR_NOT_SUPPORTED;
    *index = pin_index(pin);
    if (*index < 0) return ESP_ERR_INVALID_ARG;
    if (!initialized) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

static esp_err_t configure(int index, gpio_control_mode_t mode)
{
    known[index] = false;
    owned[index] = true;
    esp_err_t err = gpio_reset_pin(pins[index]);
    if (err != ESP_OK) return err;
    if (mode == GPIO_CONTROL_OUTPUT) {
        err = gpio_set_level(pins[index], 0);
        if (err != ESP_OK) return err;
    }
    gpio_config_t config = {
        .pin_bit_mask = UINT64_C(1) << pins[index],
        .mode = mode == GPIO_CONTROL_OUTPUT ? GPIO_MODE_INPUT_OUTPUT : GPIO_MODE_INPUT,
        .pull_up_en = mode == GPIO_CONTROL_INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = mode == GPIO_CONTROL_INPUT_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&config);
    if (err == ESP_OK) {
        states[index] = (gpio_control_state_t){.mode = mode};
        known[index] = true;
    }
    return err;
}

static esp_err_t stop_pwm(void)
{
    if (pwm_index < 0) return ESP_OK;
    esp_err_t err = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    if (err != ESP_OK) return err;
    int previous = pwm_index;
    pwm_index = -1;
    return configure(previous, GPIO_CONTROL_INPUT);
}

esp_err_t gpio_control_init(void)
{
    if (!FACTORY_BOARD_HEADER_GPIO) return ESP_ERR_NOT_SUPPORTED;
    if (initialized) return ESP_OK;
    esp_err_t err = gpio_control_cleanup();
    if (err != ESP_OK) return err;
    for (size_t i = 0; i < PIN_COUNT; ++i) {
        err = configure(i, GPIO_CONTROL_INPUT);
        if (err != ESP_OK) {
            gpio_control_cleanup();
            return err;
        }
    }
    initialized = true;
    return ESP_OK;
}

esp_err_t gpio_control_set_mode(int pin, gpio_control_mode_t mode)
{
    int index = -1;
    esp_err_t err = validate(pin, &index);
    if (err != ESP_OK) return err;
    if (mode < GPIO_CONTROL_INPUT || mode > GPIO_CONTROL_OUTPUT) return ESP_ERR_INVALID_ARG;
    if (index == pwm_index && (err = stop_pwm()) != ESP_OK) return err;
    return configure(index, mode);
}

esp_err_t gpio_control_write(int pin, int level)
{
    int index = -1;
    esp_err_t err = validate(pin, &index);
    if (err != ESP_OK) return err;
    if (level != 0 && level != 1) return ESP_ERR_INVALID_ARG;
    if (!known[index] || states[index].mode != GPIO_CONTROL_OUTPUT) return ESP_ERR_INVALID_STATE;
    err = gpio_set_level(pin, level);
    if (err == ESP_OK) states[index].level = level;
    return err;
}

esp_err_t gpio_control_read(int pin, int *level)
{
    int index = -1;
    esp_err_t err = validate(pin, &index);
    if (err != ESP_OK) return err;
    if (!level) return ESP_ERR_INVALID_ARG;
    if (!known[index]) return ESP_ERR_INVALID_STATE;
    *level = gpio_get_level(pin);
    return ESP_OK;
}

esp_err_t gpio_control_pwm(int pin, uint32_t frequency_hz, uint8_t duty_percent)
{
    int index = -1;
    esp_err_t err = validate(pin, &index);
    if (err != ESP_OK) return err;
    if (frequency_hz < 1 || frequency_hz > 20000 || duty_percent > 100) return ESP_ERR_INVALID_ARG;
    if ((err = stop_pwm()) != ESP_OK) return err;
    /* Low frequencies need more duty bits to fit LEDC's 10-bit divider.
     * Both ranges fit the board's 40/80 MHz sources without changing the
     * shared clock used by the backlight timer. */
    const unsigned resolution = frequency_hz < 100 ? 17 : 10;
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0,
        .duty_resolution = resolution, .freq_hz = frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    err = ledc_timer_config(&timer);
    if (err != ESP_OK) return err;
    timer_owned = true;
    if ((err = configure(index, GPIO_CONTROL_INPUT)) != ESP_OK) return err;
    known[index] = false;
    ledc_channel_config_t channel = {
        .gpio_num = pin, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0, .duty = ((1U << resolution) * duty_percent) / 100U,
        .hpoint = 0,
    };
    /* A full-scale duty value can overflow on some LEDC implementations. */
    if (duty_percent == 100) channel.duty = 0;
    pwm_index = index;
    err = ledc_channel_config(&channel);
    if (err == ESP_OK && (duty_percent == 0 || duty_percent == 100)) {
        err = ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_percent == 100);
    }
    if (err != ESP_OK) {
        stop_pwm();
        return err;
    }
    states[index] = (gpio_control_state_t){
        .mode = GPIO_CONTROL_PWM, .frequency_hz = frequency_hz,
        .duty_percent = duty_percent, .level = duty_percent == 100,
    };
    known[index] = true;
    return ESP_OK;
}

esp_err_t gpio_control_get_state(int pin, gpio_control_state_t *state)
{
    int index = -1;
    esp_err_t err = validate(pin, &index);
    if (err != ESP_OK) return err;
    if (!state) return ESP_ERR_INVALID_ARG;
    if (!known[index]) return ESP_ERR_INVALID_STATE;
    *state = states[index];
    if (state->mode != GPIO_CONTROL_PWM) state->level = gpio_get_level(pin);
    return ESP_OK;
}

esp_err_t gpio_control_cleanup(void)
{
    esp_err_t first = stop_pwm();
    initialized = false;
    for (size_t i = 0; i < PIN_COUNT; ++i) {
        if (!owned[i] || (int)i == pwm_index) continue;
        esp_err_t err = configure(i, GPIO_CONTROL_INPUT);
        if (err == ESP_OK) owned[i] = false;
        else if (first == ESP_OK) first = err;
    }
    if (timer_owned && pwm_index < 0) {
        esp_err_t err = ledc_timer_pause(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);
        if (err == ESP_OK) {
            const ledc_timer_config_t timer = {
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .timer_num = LEDC_TIMER_0,
                .deconfigure = true,
            };
            err = ledc_timer_config(&timer);
        }
        if (err == ESP_OK) timer_owned = false;
        else if (first == ESP_OK) first = err;
    }
    return first;
}

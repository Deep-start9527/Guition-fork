#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPIO_CONTROL_INPUT,
    GPIO_CONTROL_INPUT_PULLUP,
    GPIO_CONTROL_INPUT_PULLDOWN,
    GPIO_CONTROL_OUTPUT,
    GPIO_CONTROL_PWM,
} gpio_control_mode_t;

typedef struct {
    gpio_control_mode_t mode;
    int level;
    uint32_t frequency_hz;
    uint8_t duty_percent;
} gpio_control_state_t;

/* These calls share one controller and must be serialized by the caller. */
esp_err_t gpio_control_init(void);
esp_err_t gpio_control_set_mode(int pin, gpio_control_mode_t mode);
esp_err_t gpio_control_write(int pin, int level);
esp_err_t gpio_control_read(int pin, int *level);
esp_err_t gpio_control_pwm(int pin, uint32_t frequency_hz, uint8_t duty_percent);
esp_err_t gpio_control_get_state(int pin, gpio_control_state_t *state);
/* Idempotent; failed releases remain owned so cleanup can be retried. */
esp_err_t gpio_control_cleanup(void);

#ifdef __cplusplus
}
#endif

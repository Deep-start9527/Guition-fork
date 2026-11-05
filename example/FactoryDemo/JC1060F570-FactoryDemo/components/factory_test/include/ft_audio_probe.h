/** SPDX-License-Identifier: GPL-2.0-only */
/** Continuous dual-microphone capture and monitoring. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
#define FT_AUDIO_SAMPLE_RATE 16000U
#define FT_AUDIO_BITS 16U
#define FT_AUDIO_ADC_CHANNELS 4U
#define FT_AUDIO_DAC_CHANNELS 2U
#define FT_AUDIO_SPECTRUM_BARS 32U

typedef enum {
    FT_AUDIO_IDLE = 0,
    FT_AUDIO_STARTING,
    FT_AUDIO_CAPTURING,
    FT_AUDIO_MONITOR_LEFT,
    FT_AUDIO_MONITOR_RIGHT,
    FT_AUDIO_ERROR,
    FT_AUDIO_STOPPING,
} ft_audio_probe_state_t;

typedef enum {
    FT_AUDIO_MONITOR_OFF = 0,
    FT_AUDIO_MONITOR_SELECT_LEFT,
    FT_AUDIO_MONITOR_SELECT_RIGHT,
} ft_audio_monitor_t;

/** Start the page session; capture continues until stop or cleanup. */
esp_err_t ft_audio_probe_start(void);
/** Select TH0/TH1 (left mic/reference) or TH2/TH3 (right mic/reference); AEC output feeds both DAC channels. */
esp_err_t ft_audio_probe_monitor_set(ft_audio_monitor_t monitor);
/** Request session shutdown; the worker owns codec teardown. */
void ft_audio_probe_stop(void);
/** Wait up to three seconds. A timed-out worker retains its buffers until it exits. */
void ft_audio_probe_cleanup(void);
ft_audio_probe_state_t ft_audio_probe_state(void);
esp_err_t ft_audio_probe_last_error(void);
/** Copy TH0/TH2 levels [0,1], fixed -60..0 dBFS and 32 logarithmic bands 100..8000 Hz. */
bool ft_audio_probe_spectrum_snapshot(float levels[2][FT_AUDIO_SPECTRUM_BARS], uint32_t *sequence);
#ifdef __cplusplus
}
#endif

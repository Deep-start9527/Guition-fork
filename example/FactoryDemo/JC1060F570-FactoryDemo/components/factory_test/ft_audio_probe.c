/** SPDX-License-Identifier: GPL-2.0-only */
#include "ft_audio_probe.h"

#include <math.h>
#include <string.h>
#include "audio_input.h"
#include "audio_output.h"
#include "audio_runtime.h"
#include "esp_codec_dev.h"
#include "esp_afe_aec.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "spectrum_fft.h"

#define FT_AUDIO_TAG "FactoryTestAudio"
#define FT_AUDIO_AEC_FILTER_LENGTH 4
#define FT_AUDIO_WORKER_STACK 6144U
#define FT_AUDIO_WORKER_PRIORITY 5
#define FT_AUDIO_SETTLE_TIMEOUT_MS 3000U
#define FT_AUDIO_SETTLE_STEP_MS 10U

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_running;
static bool s_stop_requested;
static ft_audio_probe_state_t s_state = FT_AUDIO_IDLE;
static esp_err_t s_last_error = ESP_OK;
static uint32_t s_sequence;
static float s_levels[2][FT_AUDIO_SPECTRUM_BARS];

typedef struct {
    bool runtime_acquired;
    bool output_was_open;
    bool output_reconfigured;
    bool input_opened;
    esp_codec_dev_sample_info_t previous_output_format;
} audio_session_t;

static esp_err_t session_begin(audio_session_t *session)
{
    memset(session, 0, sizeof(*session));
    esp_err_t result = audio_runtime_acquire();
    if (result != ESP_OK) return result;
    session->runtime_acquired = true;
    /* An existing capture belongs to another page and must not be closed here. */
    if (audio_input_is_open()) return ESP_ERR_INVALID_STATE;
    session->output_was_open = audio_output_is_open();
    if (session->output_was_open &&
        audio_output_get_format(&session->previous_output_format) != ESP_OK) return ESP_FAIL;
    const esp_codec_dev_sample_info_t output = {
        .sample_rate = FT_AUDIO_SAMPLE_RATE, .channel = FT_AUDIO_DAC_CHANNELS,
        .bits_per_sample = FT_AUDIO_BITS,
    };
    result = audio_output_reconfigure(&output);
    if (result != ESP_OK) return result;
    session->output_reconfigured = true;
    const esp_codec_dev_sample_info_t input = {
        .sample_rate = FT_AUDIO_SAMPLE_RATE, .channel = FT_AUDIO_ADC_CHANNELS,
        .bits_per_sample = FT_AUDIO_BITS,
    };
    result = audio_input_open(&input);
    session->input_opened = result == ESP_OK;
    return result;
}

static esp_err_t session_end(audio_session_t *session)
{
    esp_err_t result = ESP_OK;
    if (session->input_opened && audio_input_close() != ESP_OK) result = ESP_FAIL;
    if (session->output_reconfigured) {
        esp_err_t restored = session->output_was_open
            ? audio_output_reconfigure(&session->previous_output_format) : audio_output_close();
        if (restored != ESP_OK) result = restored;
    }
    if (session->runtime_acquired && audio_runtime_release() != ESP_OK) result = ESP_FAIL;
    return result;
}

static void publish_spectrum(const spectrum_fft_t *fft)
{
    float levels[2][FT_AUDIO_SPECTRUM_BARS];
    for (unsigned mic = 0; mic < 2; ++mic) {
        const float *magnitudes = spectrum_fft_microphone_magnitudes(fft, mic);
        for (unsigned bar = 0; bar < FT_AUDIO_SPECTRUM_BARS; ++bar) {
            const float low = 100.0f * powf(80.0f, (float)bar / FT_AUDIO_SPECTRUM_BARS);
            const float high = 100.0f * powf(80.0f, (float)(bar + 1) / FT_AUDIO_SPECTRUM_BARS);
            unsigned first = (unsigned)ceilf(low * SPECTRUM_FFT_SIZE / FT_AUDIO_SAMPLE_RATE);
            unsigned end = (unsigned)ceilf(high * SPECTRUM_FFT_SIZE / FT_AUDIO_SAMPLE_RATE);
            if (end > SPECTRUM_FFT_BINS) end = SPECTRUM_FFT_BINS;
            float peak = 0.0f;
            for (unsigned bin = first; bin < end; ++bin) {
                if (magnitudes[bin] > peak) peak = magnitudes[bin];
            }
            float level = peak > 0.001f ? (20.0f * log10f(peak) + 60.0f) / 60.0f : 0.0f;
            levels[mic][bar] = level > 1.0f ? 1.0f : level;
        }
    }
    portENTER_CRITICAL(&s_lock);
    memcpy(s_levels, levels, sizeof(s_levels));
    ++s_sequence;
    portEXIT_CRITICAL(&s_lock);
}

static void audio_worker(void *argument)
{
    (void)argument;
    afe_aec_handle_t *aec[2] = {NULL, NULL};
    int16_t *clean[2] = {NULL, NULL};
    int16_t *capture = NULL;
    int16_t *output = NULL;
    spectrum_fft_t *fft = NULL;
    size_t frames = 0;
    audio_session_t session = {0};
    esp_err_t result = ESP_ERR_NO_MEM;
    aec[0] = afe_aec_create("MRNN", FT_AUDIO_AEC_FILTER_LENGTH,
                            AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    if (aec[0]) {
        aec[1] = afe_aec_create("NNMR", FT_AUDIO_AEC_FILTER_LENGTH,
                                AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    }
    if (aec[0] && aec[1]) {
        const int chunk = afe_aec_get_chunksize(aec[0]);
        /* Bound allocation sizes and reject incompatible stereo capture framing. */
        if (chunk <= 0 || chunk > (int)FT_AUDIO_SAMPLE_RATE ||
            chunk != afe_aec_get_chunksize(aec[1])) {
            result = ESP_ERR_INVALID_STATE;
        } else {
            frames = (size_t)chunk;
            capture = heap_caps_aligned_calloc(16, frames * FT_AUDIO_ADC_CHANNELS,
                                               sizeof(int16_t), MALLOC_CAP_SPIRAM);
            output = heap_caps_aligned_calloc(16, frames * FT_AUDIO_DAC_CHANNELS,
                                              sizeof(int16_t), MALLOC_CAP_SPIRAM);
            for (unsigned mic = 0; mic < 2; ++mic) {
                clean[mic] = heap_caps_aligned_calloc(16, frames, sizeof(int16_t), MALLOC_CAP_SPIRAM);
            }
            fft = heap_caps_malloc(sizeof(*fft), MALLOC_CAP_SPIRAM);
            if (capture && output && clean[0] && clean[1] && fft &&
                spectrum_fft_init(fft, FT_AUDIO_SAMPLE_RATE, FT_AUDIO_ADC_CHANNELS, FT_AUDIO_BITS)) {
                result = session_begin(&session);
            }
        }
    }
    esp_codec_dev_handle_t input_codec = audio_input_get_codec_dev();
    esp_codec_dev_handle_t output_codec = audio_output_get_codec_dev();
    if (result == ESP_OK && (!input_codec || !output_codec)) result = ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&s_lock);
    if (result == ESP_OK && !s_stop_requested) s_state = FT_AUDIO_CAPTURING;
    portEXIT_CRITICAL(&s_lock);
    uint32_t windows = 0;
    while (result == ESP_OK) {
        portENTER_CRITICAL(&s_lock);
        bool stop = s_stop_requested;
        portEXIT_CRITICAL(&s_lock);
        if (stop) break;
        if (esp_codec_dev_read(input_codec, capture,
                frames * FT_AUDIO_ADC_CHANNELS * sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
            result = ESP_FAIL;
            break;
        }
        /* Independent filters keep each microphone paired with its hardware reference. */
        for (unsigned mic = 0; mic < 2; ++mic) {
            if (afe_aec_process(aec[mic], capture, clean[mic]) != frames * sizeof(int16_t)) {
                result = ESP_FAIL;
                break;
            }
        }
        if (result != ESP_OK) break;
        portENTER_CRITICAL(&s_lock);
        ft_audio_probe_state_t mode = s_stop_requested ? FT_AUDIO_STOPPING : s_state;
        portEXIT_CRITICAL(&s_lock);
        memset(output, 0, frames * FT_AUDIO_DAC_CHANNELS * sizeof(int16_t));
        for (size_t frame = 0; frame < frames; ++frame) {
            int16_t sample = 0;
            if (mode == FT_AUDIO_MONITOR_LEFT || mode == FT_AUDIO_MONITOR_RIGHT) {
                sample = clean[mode == FT_AUDIO_MONITOR_RIGHT ? 1U : 0U][frame];
            }
            output[frame * 2] = sample;
            output[frame * 2 + 1] = sample;
        }
        /** Keep TX supplied even during capture so the shared full-duplex clock remains active. */
        if (esp_codec_dev_write(output_codec, output,
                frames * FT_AUDIO_DAC_CHANNELS * sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
            result = ESP_FAIL;
            break;
        }
        spectrum_fft_feed(fft, (const uint8_t *)capture,
                          frames * FT_AUDIO_ADC_CHANNELS * sizeof(int16_t));
        if (fft->windows_processed != windows) {
            windows = fft->windows_processed;
            publish_spectrum(fft);
        }
    }
    /* Flush silence after monitoring so queued DAC data cannot repeat on the next page. */
    if (session.output_reconfigured && output_codec && output) {
        memset(output, 0, frames * FT_AUDIO_DAC_CHANNELS * sizeof(int16_t));
        if (esp_codec_dev_write(output_codec, output,
                frames * FT_AUDIO_DAC_CHANNELS * sizeof(int16_t)) != ESP_CODEC_DEV_OK && result == ESP_OK) result = ESP_FAIL;
    }
    esp_err_t teardown = session_end(&session);
    if (result == ESP_OK) result = teardown;
    for (unsigned mic = 0; mic < 2; ++mic) {
        if (aec[mic]) afe_aec_destroy(aec[mic]);
        heap_caps_free(clean[mic]);
    }
    heap_caps_free(capture);
    heap_caps_free(output);
    heap_caps_free(fft);
    portENTER_CRITICAL(&s_lock);
    s_last_error = result;
    s_state = result == ESP_OK ? FT_AUDIO_IDLE : FT_AUDIO_ERROR;
    s_running = false;
    portEXIT_CRITICAL(&s_lock);
    vTaskDeleteWithCaps(NULL);
}

esp_err_t ft_audio_probe_start(void)
{
    portENTER_CRITICAL(&s_lock);
    if (s_running) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_running = true;
    s_stop_requested = false;
    s_last_error = ESP_OK;
    s_state = FT_AUDIO_STARTING;
    s_sequence = 0;
    memset(s_levels, 0, sizeof(s_levels));
    portEXIT_CRITICAL(&s_lock);
    if (xTaskCreatePinnedToCoreWithCaps(audio_worker, "ft_audio", FT_AUDIO_WORKER_STACK, NULL,
            FT_AUDIO_WORKER_PRIORITY, NULL, 1, MALLOC_CAP_SPIRAM) != pdPASS) {
        portENTER_CRITICAL(&s_lock);
        s_running = false;
        s_state = FT_AUDIO_ERROR;
        s_last_error = ESP_ERR_NO_MEM;
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t request_mode(ft_audio_probe_state_t mode)
{
    portENTER_CRITICAL(&s_lock);
    if (!s_running || s_stop_requested ||
        (s_state != FT_AUDIO_CAPTURING && s_state != FT_AUDIO_MONITOR_LEFT && s_state != FT_AUDIO_MONITOR_RIGHT)) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_state = mode;
    portEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t ft_audio_probe_monitor_set(ft_audio_monitor_t monitor)
{
    switch (monitor) {
    case FT_AUDIO_MONITOR_OFF: return request_mode(FT_AUDIO_CAPTURING);
    case FT_AUDIO_MONITOR_SELECT_LEFT: return request_mode(FT_AUDIO_MONITOR_LEFT);
    case FT_AUDIO_MONITOR_SELECT_RIGHT: return request_mode(FT_AUDIO_MONITOR_RIGHT);
    default: return ESP_ERR_INVALID_ARG;
    }
}
void ft_audio_probe_stop(void)
{
    portENTER_CRITICAL(&s_lock);
    if (s_running) {
        s_stop_requested = true;
        s_state = FT_AUDIO_STOPPING;
    }
    portEXIT_CRITICAL(&s_lock);
}
void ft_audio_probe_cleanup(void)
{
    ft_audio_probe_stop();
    for (uint32_t waited = 0; waited < FT_AUDIO_SETTLE_TIMEOUT_MS; waited += FT_AUDIO_SETTLE_STEP_MS) {
        portENTER_CRITICAL(&s_lock);
        bool running = s_running;
        portEXIT_CRITICAL(&s_lock);
        if (!running) return;
        vTaskDelay(pdMS_TO_TICKS(FT_AUDIO_SETTLE_STEP_MS));
    }
    ESP_LOGE(FT_AUDIO_TAG, "audio shutdown timed out; worker retains ownership");
}
ft_audio_probe_state_t ft_audio_probe_state(void)
{
    portENTER_CRITICAL(&s_lock);
    ft_audio_probe_state_t value = s_state;
    portEXIT_CRITICAL(&s_lock);
    return value;
}
esp_err_t ft_audio_probe_last_error(void)
{
    portENTER_CRITICAL(&s_lock);
    esp_err_t value = s_last_error;
    portEXIT_CRITICAL(&s_lock);
    return value;
}
bool ft_audio_probe_spectrum_snapshot(float levels[2][FT_AUDIO_SPECTRUM_BARS], uint32_t *sequence)
{
    if (!levels) return false;
    portENTER_CRITICAL(&s_lock);
    memcpy(levels, s_levels, sizeof(s_levels));
    if (sequence) *sequence = s_sequence;
    bool available = s_sequence != 0;
    portEXIT_CRITICAL(&s_lock);
    return available;
}

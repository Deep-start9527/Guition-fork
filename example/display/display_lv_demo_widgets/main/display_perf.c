#include "display_perf.h"

#if CONFIG_DEMO_DISPLAY_PERF_STATS
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#define PERF_WINDOW_MS 5000
#define PERF_HISTOGRAM_MS 1000
#define PERF_FLUSH_CALLBACK 1u
#define PERF_FLUSH_WAIT 2u

typedef struct {
    lv_timer_t *timer;
    int64_t window_start_us;
    int64_t refresh_start_us;
    int64_t flush_start_us;
    uint64_t refresh_flush_us;
    uint64_t flush_us;
    unsigned flush_phases;
    uint64_t total_us;
    uint64_t max_us;
    uint32_t refreshes;
    /* Millisecond upper bounds, plus one overflow bucket. */
    uint32_t histogram[PERF_HISTOGRAM_MS + 2];
    bool active;
    bool rendered;
} display_perf_t;

static const char *TAG = "display_perf";

static unsigned percentile95_ms(const display_perf_t *stats)
{
    uint64_t rank = ((uint64_t)stats->refreshes * 95 + 99) / 100;
    uint64_t cumulative = 0;
    for (unsigned i = 0; i < PERF_HISTOGRAM_MS + 2; ++i) {
        cumulative += stats->histogram[i];
        if (cumulative >= rank) {
            return i;
        }
    }
    return PERF_HISTOGRAM_MS + 1;
}

static void report_window(lv_timer_t *timer)
{
    display_perf_t *stats = lv_timer_get_user_data(timer);
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - stats->window_start_us;
    if (elapsed <= 0) {
        return;
    }
    unsigned p95 = percentile95_ms(stats);
    bool overflow = p95 > PERF_HISTOGRAM_MS;
    ESP_LOGI(TAG, "LVGL refresh: count=%lu rate=%.2f Hz processing_avg=%.3f ms "
             "flush_avg=%.3f ms render_other_avg=%.3f ms "
             "p95%s%u ms (1ms buckets) max=%.3f ms window=%.3f s; "
             "free_internal=%u free_psram=%u bytes",
             (unsigned long)stats->refreshes,
             (double)stats->refreshes * 1000000.0 / elapsed,
             stats->refreshes ? (double)stats->total_us / stats->refreshes / 1000.0 : 0.0,
             stats->refreshes ? (double)stats->flush_us / stats->refreshes / 1000.0 : 0.0,
             stats->refreshes ? (double)(stats->total_us - stats->flush_us) / stats->refreshes / 1000.0 : 0.0,
             overflow ? ">" : "<=", overflow ? PERF_HISTOGRAM_MS : p95,
             (double)stats->max_us / 1000.0, (double)elapsed / 1000000.0,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    stats->window_start_us = now;
    stats->refreshes = 0;
    stats->total_us = 0;
    stats->flush_us = 0;
    stats->max_us = 0;
    memset(stats->histogram, 0, sizeof(stats->histogram));
}

/* Count the union of callback and wait intervals: a callback can itself wait.
 * LVGL also waits before FLUSH_START when reusing a double buffer. */
static void flush_phase(display_perf_t *stats, unsigned phase, bool start)
{
    if (!stats->active) {
        return;
    }
    if (start) {
        if (!stats->flush_phases) {
            stats->flush_start_us = esp_timer_get_time();
        }
        stats->flush_phases |= phase;
    } else if (stats->flush_phases & phase) {
        stats->flush_phases &= ~phase;
        if (!stats->flush_phases) {
            stats->refresh_flush_us += (uint64_t)(esp_timer_get_time() - stats->flush_start_us);
        }
    }
}

static void display_event(lv_event_t *event)
{
    display_perf_t *stats = lv_event_get_user_data(event);
    switch (lv_event_get_code(event)) {
    case LV_EVENT_REFR_START:
        stats->refresh_start_us = esp_timer_get_time();
        stats->refresh_flush_us = 0;
        stats->flush_phases = 0;
        stats->active = true;
        stats->rendered = false;
        break;
    case LV_EVENT_RENDER_START:
        if (stats->active) {
            stats->rendered = true;
        }
        break;
    case LV_EVENT_FLUSH_START:
        flush_phase(stats, PERF_FLUSH_CALLBACK, true);
        break;
    case LV_EVENT_FLUSH_FINISH:
        flush_phase(stats, PERF_FLUSH_CALLBACK, false);
        break;
    case LV_EVENT_FLUSH_WAIT_START:
        flush_phase(stats, PERF_FLUSH_WAIT, true);
        break;
    case LV_EVENT_FLUSH_WAIT_FINISH:
        flush_phase(stats, PERF_FLUSH_WAIT, false);
        break;
    case LV_EVENT_REFR_READY:
        if (stats->active && stats->rendered) {
            int64_t now = esp_timer_get_time();
            uint64_t duration = (uint64_t)(now - stats->refresh_start_us);
            /* Bound unfinished intervals to this refresh, never to scanout. */
            if (stats->flush_phases) {
                stats->refresh_flush_us += (uint64_t)(now - stats->flush_start_us);
            }
            stats->flush_us += stats->refresh_flush_us;
            uint64_t bucket = (duration + 999) / 1000;
            if (bucket > PERF_HISTOGRAM_MS) {
                bucket = PERF_HISTOGRAM_MS + 1;
            }
            ++stats->histogram[bucket];
            ++stats->refreshes;
            stats->total_us += duration;
            if (duration > stats->max_us) {
                stats->max_us = duration;
            }
        }
        stats->flush_phases = 0;
        stats->active = false;
        stats->rendered = false;
        break;
    case LV_EVENT_DELETE:
        lv_timer_delete(stats->timer);
        free(stats);
        break;
    default:
        break;
    }
}

esp_err_t demo_display_perf_attach(lv_display_t *display)
{
    if (display == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    display_perf_t *stats = calloc(1, sizeof(*stats));
    if (stats == NULL) {
        return ESP_ERR_NO_MEM;
    }
    stats->window_start_us = esp_timer_get_time();
    stats->timer = lv_timer_create(report_window, PERF_WINDOW_MS, stats);
    if (stats->timer == NULL) {
        free(stats);
        return ESP_ERR_NO_MEM;
    }
    if (lv_display_add_event_cb(display, display_event, LV_EVENT_ALL, stats) == NULL) {
        lv_timer_delete(stats->timer);
        free(stats);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
#endif

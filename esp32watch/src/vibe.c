#include "vibe.h"
#include "drv_vibrator.h"
#include "platform_api.h"

typedef enum {
    VIBE_PATTERN_NONE = 0,
    VIBE_PATTERN_SOFT,
    VIBE_PATTERN_CONFIRM,
    VIBE_PATTERN_ALARM,
    VIBE_PATTERN_GOAL
} VibePattern;

static uint32_t g_pulse_until;
static uint32_t g_next_toggle_ms;
static uint8_t g_pulses_remaining;
static bool g_output_active;
static VibePattern g_pattern;

static void vibe_schedule_pattern(VibePattern pattern)
{
    g_pattern = pattern;
    g_output_active = true;
    drv_vibrator_set(true);
    g_next_toggle_ms = platform_time_now_ms() + 20U;

    switch (pattern) {
        case VIBE_PATTERN_SOFT:
            g_pulses_remaining = 1U;
            g_pulse_until = platform_time_now_ms() + 18U;
            break;
        case VIBE_PATTERN_CONFIRM:
            g_pulses_remaining = 2U;
            g_pulse_until = platform_time_now_ms() + 30U;
            break;
        case VIBE_PATTERN_ALARM:
            g_pulses_remaining = 6U;
            g_pulse_until = platform_time_now_ms() + 80U;
            break;
        case VIBE_PATTERN_GOAL:
            g_pulses_remaining = 3U;
            g_pulse_until = platform_time_now_ms() + 40U;
            break;
        default:
            g_pulses_remaining = 0U;
            g_pulse_until = 0U;
            g_output_active = false;
            drv_vibrator_set(false);
            break;
    }
}

void vibe_init(void)
{
    g_pulse_until = 0U;
    g_next_toggle_ms = 0U;
    g_pulses_remaining = 0U;
    g_output_active = false;
    g_pattern = VIBE_PATTERN_NONE;
    drv_vibrator_init();
    drv_vibrator_set(false);
}

void vibe_tick(uint32_t now_ms, PopupType popup, bool enabled)
{
    if (!enabled) {
        g_pulse_until = 0U;
        g_next_toggle_ms = 0U;
        g_pulses_remaining = 0U;
        g_output_active = false;
        g_pattern = VIBE_PATTERN_NONE;
        drv_vibrator_set(false);
        return;
    }

    if ((popup == POPUP_ALARM) && g_pattern != VIBE_PATTERN_ALARM && g_pulses_remaining == 0U) {
        vibe_schedule_pattern(VIBE_PATTERN_ALARM);
    } else if ((popup == POPUP_GOAL_REACHED) && g_pattern == VIBE_PATTERN_NONE) {
        vibe_schedule_pattern(VIBE_PATTERN_GOAL);
    }

    if (g_pulses_remaining == 0U) {
        return;
    }

    if (g_output_active && now_ms >= g_next_toggle_ms) {
        drv_vibrator_set(false);
        g_output_active = false;
        if (g_pulses_remaining > 0U) {
            g_pulses_remaining--;
        }
        if (g_pulses_remaining == 0U) {
            g_pattern = VIBE_PATTERN_NONE;
            g_pulse_until = 0U;
        } else {
            g_next_toggle_ms = now_ms + 60U;
        }
    } else if (!g_output_active && g_pulses_remaining > 0U && now_ms >= g_next_toggle_ms) {
        drv_vibrator_set(true);
        g_output_active = true;
        g_next_toggle_ms = now_ms + ((g_pattern == VIBE_PATTERN_ALARM) ? 70U : 20U);
    }

    if (g_pulse_until != 0U && now_ms >= g_pulse_until && g_pattern != VIBE_PATTERN_ALARM) {
        drv_vibrator_set(false);
        g_output_active = false;
        g_pulses_remaining = 0U;
        g_pattern = VIBE_PATTERN_NONE;
        g_pulse_until = 0U;
    }
}

void vibe_pulse(uint16_t duration_ms)
{
    if (duration_ms >= 28U) {
        vibe_schedule_pattern(VIBE_PATTERN_CONFIRM);
    } else {
        vibe_schedule_pattern(VIBE_PATTERN_SOFT);
    }
    g_pulse_until = platform_time_now_ms() + duration_ms;
}


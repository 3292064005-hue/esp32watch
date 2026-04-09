#include "drv_buzzer.h"
#include "board_manifest.h"
#include <Arduino.h>

static bool g_buzzer_available = false;
static bool g_buzzer_active = false;
static bool g_buzzer_playback_mode = false;
static int g_buzzer_gpio = -1;
static bool g_buzzer_output_high = false;
static uint16_t g_buzzer_tone_hz = 0U;
static uint32_t g_buzzer_next_toggle_us = 0U;

static void buzzer_write_level(bool high)
{
    if (g_buzzer_gpio < 0) {
        return;
    }
    digitalWrite(g_buzzer_gpio, high ? HIGH : LOW);
    g_buzzer_output_high = high;
}

static void buzzer_force_idle_low(void)
{
    buzzer_write_level(false);
    g_buzzer_next_toggle_us = 0U;
}

extern "C" void drv_buzzer_init(void)
{
    const BoardManifest *manifest = board_manifest_get();

    g_buzzer_available = false;
    g_buzzer_active = false;
    g_buzzer_playback_mode = false;
    g_buzzer_gpio = -1;
    g_buzzer_output_high = false;
    g_buzzer_tone_hz = 0U;
    g_buzzer_next_toggle_us = 0U;

    if (manifest == nullptr || !manifest->buzzer_enabled || manifest->buzzer_gpio < 0) {
        return;
    }

    g_buzzer_gpio = manifest->buzzer_gpio;
    pinMode(g_buzzer_gpio, OUTPUT);
    digitalWrite(g_buzzer_gpio, LOW);
    buzzer_force_idle_low();
    g_buzzer_available = true;
}

extern "C" void drv_buzzer_tick(uint32_t now_ms)
{
    uint32_t now_us;
    uint32_t half_period_us;

    (void)now_ms;
    if (!g_buzzer_available || g_buzzer_gpio < 0) {
        return;
    }
    if (!g_buzzer_active || g_buzzer_tone_hz == 0U) {
        return;
    }
    now_us = micros();
    if (g_buzzer_next_toggle_us == 0U) {
        half_period_us = 500000UL / (uint32_t)g_buzzer_tone_hz;
        if (half_period_us == 0U) {
            half_period_us = 1U;
        }
        g_buzzer_next_toggle_us = now_us + half_period_us;
        return;
    }
    if ((int32_t)(now_us - g_buzzer_next_toggle_us) < 0) {
        return;
    }

    half_period_us = 500000UL / (uint32_t)g_buzzer_tone_hz;
    if (half_period_us == 0U) {
        half_period_us = 1U;
    }
    buzzer_write_level(!g_buzzer_output_high);
    g_buzzer_next_toggle_us = now_us + half_period_us;
}

extern "C" void drv_buzzer_set_playback_mode(bool enabled)
{
    if (!g_buzzer_available || g_buzzer_gpio < 0) {
        return;
    }

    g_buzzer_playback_mode = enabled;
    g_buzzer_active = false;
    g_buzzer_tone_hz = 0U;
    buzzer_write_level(false);
    g_buzzer_next_toggle_us = 0U;
}

extern "C" void drv_buzzer_play_tone(uint16_t hz)
{
    if (!g_buzzer_available || g_buzzer_gpio < 0) {
        return;
    }

    if (hz == 0U) {
        g_buzzer_active = false;
        g_buzzer_tone_hz = 0U;
        if (g_buzzer_playback_mode) {
            buzzer_write_level(false);
            g_buzzer_next_toggle_us = 0U;
        } else {
            buzzer_force_idle_low();
        }
        return;
    }

    g_buzzer_tone_hz = hz;
    g_buzzer_active = true;
    buzzer_write_level(true);
    g_buzzer_next_toggle_us = micros() + (500000UL / (uint32_t)hz);
}

extern "C" void drv_buzzer_stop(void)
{
    if (!g_buzzer_available || g_buzzer_gpio < 0) {
        return;
    }

    buzzer_force_idle_low();
    g_buzzer_active = false;
    g_buzzer_tone_hz = 0U;
}

extern "C" bool drv_buzzer_is_available(void)
{
    return g_buzzer_available;
}

extern "C" bool drv_buzzer_is_active(void)
{
    return g_buzzer_active;
}

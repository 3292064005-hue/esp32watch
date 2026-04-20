#include <Preferences.h>
#include <string.h>

extern "C" {
#include "persist.h"
#include "main.h"
#include "app_config.h"
#include "board_features.h"
#include "common/storage_codec.h"
#include "persist_namespace_config.h"
}
#include "persist_preferences.h"
#include "persist_codec.h"
#include "persist_internal.hpp"

static bool persist_settings_open(bool read_only)
{
    return persist_preferences_begin(g_settings_prefs, PERSIST_PREFS_DOMAIN_APP_SETTINGS, read_only);
}

void rtc_mirror_settings(const SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SETTINGS0, storage_pack_settings0(settings));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SETTINGS1, storage_pack_settings1(settings));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SETTINGS2, storage_pack_settings2(settings));
}

bool durable_store_settings(const SettingsState *settings)
{
    if (!durable_app_state_enabled() || settings == NULL) {
        return false;
    }
    DurableSettingsRecord record = persist_codec_build_settings_record(settings, PERSIST_SETTINGS_RECORD_MAGIC, APP_STORAGE_VERSION);
    if (!persist_settings_open(false)) {
        return false;
    }
    const bool ok = g_settings_prefs.putBytes(PERSIST_SETTINGS_RECORD_KEY, &record, sizeof(record)) == sizeof(record);
    g_settings_prefs.end();
    rtc_mirror_settings(settings);
    return ok;
}

bool durable_load_settings(SettingsState *settings)
{
    DurableSettingsRecord record = {};
    if (!durable_app_state_enabled() || settings == NULL) {
        return false;
    }
    if (!persist_settings_open(true)) {
        return false;
    }
    const bool loaded = g_settings_prefs.getBytes(PERSIST_SETTINGS_RECORD_KEY, &record, sizeof(record)) == sizeof(record) &&
                        persist_codec_settings_record_valid(&record, PERSIST_SETTINGS_RECORD_MAGIC, APP_STORAGE_VERSION);
    g_settings_prefs.end();
    if (!loaded) {
        return false;
    }
    storage_unpack_settings0(record.settings0, settings);
    settings->goal = record.settings1;
    if (settings->goal < 1000U || settings->goal > 30000U) {
        settings->goal = DEFAULT_STEP_GOAL;
    }
    if (settings->screen_timeout_idx > 2U) {
        settings->screen_timeout_idx = 0U;
    }
    if (settings->sensor_sensitivity > 2U) {
        settings->sensor_sensitivity = DEFAULT_SENSOR_SENSITIVITY;
    }
    if (settings->brightness > 3U) {
        settings->brightness = DEFAULT_BRIGHTNESS;
    }
    if (settings->watchface > 2U) {
        settings->watchface = DEFAULT_WATCHFACE;
    }
    rtc_mirror_settings(settings);
    return true;
}

extern "C" void persist_save_settings(const SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
    if (!durable_store_settings(settings)) {
        rtc_mirror_settings(settings);
    }
}

extern "C" void persist_load_settings(SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
    if (!durable_load_settings(settings)) {
        storage_unpack_settings0((uint16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS0), settings);
        settings->goal = (uint16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS1);
        if (settings->goal < 1000U || settings->goal > 30000U) {
            settings->goal = DEFAULT_STEP_GOAL;
        }
        if (settings->screen_timeout_idx > 2U) {
            settings->screen_timeout_idx = 0U;
        }
        if (settings->sensor_sensitivity > 2U) {
            settings->sensor_sensitivity = DEFAULT_SENSOR_SENSITIVITY;
        }
        if (settings->brightness > 3U) {
            settings->brightness = DEFAULT_BRIGHTNESS;
        }
        if (settings->watchface > 2U) {
            settings->watchface = DEFAULT_WATCHFACE;
        }
    }
}

#include "storage_schema.h"
#include "app_config.h"
#include "common/storage_codec.h"
#include <string.h>

static void seed_default_settings(SettingsState *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->brightness = DEFAULT_BRIGHTNESS;
    settings->vibrate = false;
    settings->auto_wake = DEFAULT_AUTO_WAKE != 0U;
    settings->auto_sleep = DEFAULT_AUTO_SLEEP != 0U;
    settings->dnd = DEFAULT_DND != 0U;
    settings->show_seconds = DEFAULT_SHOW_SECONDS != 0U;
    settings->animations = DEFAULT_ANIMATIONS != 0U;
    settings->watchface = DEFAULT_WATCHFACE;
    settings->screen_timeout_idx = 0U;
    settings->sensor_sensitivity = DEFAULT_SENSOR_SENSITIVITY;
    settings->goal = DEFAULT_STEP_GOAL;
}

static void seed_default_alarm(AlarmState *alarm, uint8_t index)
{
    memset(alarm, 0, sizeof(*alarm));
    alarm->enabled = false;
    alarm->hour = DEFAULT_ALARM_HOUR;
    alarm->minute = DEFAULT_ALARM_MINUTE;
    alarm->repeat_mask = 0x7FU;
    alarm->label_id = index;
}

size_t storage_schema_payload_size_for_version(uint8_t version)
{
    switch (version) {
        case STORAGE_SCHEMA_VERSION_V4: return sizeof(StorageSchemaPayloadV4);
        case STORAGE_SCHEMA_VERSION_V5: return sizeof(StorageSchemaPayloadV5);
        case STORAGE_SCHEMA_VERSION_CURRENT: return sizeof(StorageSchemaPayloadV6);
        default: return 0U;
    }
}

bool storage_schema_validate_payload(const StorageSchemaPayload *payload)
{
    SettingsState settings;
    AlarmState alarms[APP_MAX_ALARMS] = {0};
    SensorCalibrationData cal;
    if (payload == NULL) {
        return false;
    }
    storage_schema_to_runtime(payload, &settings, alarms, APP_MAX_ALARMS, &cal, NULL);
    (void)cal;
    if (settings.goal < 1000U || settings.goal > 30000U) {
        return false;
    }
    if (settings.brightness > 3U || settings.screen_timeout_idx > 2U || settings.sensor_sensitivity > 2U) {
        return false;
    }
    for (uint8_t i = 0U; i < APP_MAX_ALARMS; ++i) {
        if (alarms[i].hour > 23U || alarms[i].minute > 59U) {
            return false;
        }
        if (alarms[i].repeat_mask > 0x7FU) {
            return false;
        }
    }
    return true;
}

static bool migrate_v5_to_current(const StorageSchemaPayloadV5 *src, StorageSchemaPayload *dst)
{
    if (src == NULL || dst == NULL) {
        return false;
    }

    memset(dst, 0, sizeof(*dst));
    dst->settings0 = src->settings0;
    dst->settings1 = src->settings1;
    dst->settings2 = src->settings2;
    memcpy(dst->alarms, src->alarms, sizeof(dst->alarms));
    dst->alarm_meta = src->alarm_meta;
    memcpy(dst->cal, src->cal, sizeof(dst->cal));
    dst->reserved[0] = src->reserved[0];
    dst->reserved[1] = src->reserved[1];
    return true;
}

static bool migrate_v4_to_current(const StorageSchemaPayloadV4 *src, StorageSchemaPayload *dst)
{
    if (src == NULL || dst == NULL) {
        return false;
    }
    memset(dst, 0, sizeof(*dst));
    dst->settings0 = src->settings0;
    dst->settings1 = src->settings1;
    dst->settings2 = 0xA500U | (uint16_t)(APP_CLAMP(src->settings1 / 500U, 0U, 60U) & 0x003FU);
    memcpy(dst->alarms, src->alarms, sizeof(dst->alarms));
    dst->alarm_meta = src->alarm_meta;
    memcpy(dst->cal, src->cal, sizeof(dst->cal));
    dst->reserved[0] = src->reserved[0];
    dst->reserved[1] = src->reserved[1];
    return true;
}

bool storage_schema_upgrade_payload(uint8_t src_version,
                                    const void *src_payload,
                                    size_t src_len,
                                    StorageSchemaPayload *dst_payload)
{
    if (src_payload == NULL || dst_payload == NULL) {
        return false;
    }
    if (src_version == STORAGE_SCHEMA_VERSION_CURRENT) {
        if (src_len != sizeof(StorageSchemaPayload)) {
            return false;
        }
        memcpy(dst_payload, src_payload, sizeof(StorageSchemaPayload));
        return storage_schema_validate_payload(dst_payload);
    }
    if (src_version == STORAGE_SCHEMA_VERSION_V5) {
        StorageSchemaPayload migrated;
        if (src_len != sizeof(StorageSchemaPayloadV5)) {
            return false;
        }
        if (!migrate_v5_to_current((const StorageSchemaPayloadV5 *)src_payload, &migrated)) {
            return false;
        }
        *dst_payload = migrated;
        return storage_schema_validate_payload(dst_payload);
    }
    if (src_version == STORAGE_SCHEMA_VERSION_V4) {
        StorageSchemaPayload migrated;
        if (src_len != sizeof(StorageSchemaPayloadV4)) {
            return false;
        }
        if (!migrate_v4_to_current((const StorageSchemaPayloadV4 *)src_payload, &migrated)) {
            return false;
        }
        *dst_payload = migrated;
        return storage_schema_validate_payload(dst_payload);
    }
    return false;
}

void storage_schema_from_runtime(StorageSchemaPayload *payload,
                                 const SettingsState *settings,
                                 const AlarmState *alarms,
                                 const SensorCalibrationData *cal,
                                 const GameStatsState *game_stats)
{
    SensorCalibrationData local_cal;
    AlarmState local_alarms[APP_MAX_ALARMS] = {0};
    SettingsState local_settings;
    GameStatsState local_game_stats;

    if (payload == NULL || settings == NULL || alarms == NULL || cal == NULL || game_stats == NULL) {
        return;
    }

    memset(payload, 0, sizeof(*payload));
    local_settings = *settings;
    local_cal = *cal;
    local_game_stats = *game_stats;
    for (uint8_t i = 0U; i < APP_MAX_ALARMS; ++i) {
        local_alarms[i] = alarms[i];
    }

    payload->settings0 = storage_pack_settings0(&local_settings);
    payload->settings1 = storage_pack_settings1(&local_settings);
    payload->settings2 = storage_pack_settings2(&local_settings);
    for (uint8_t i = 0U; i < APP_MAX_ALARMS; ++i) {
        payload->alarms[i] = storage_pack_alarm_word(&local_alarms[i]);
    }
    payload->alarm_meta = storage_pack_alarm_meta(&local_alarms[0], &local_alarms[1], &local_alarms[2]);
    payload->cal[0] = local_cal.az_bias;
    payload->cal[1] = local_cal.gx_bias;
    payload->cal[2] = local_cal.gy_bias;
    payload->cal[3] = local_cal.gz_bias;
    payload->breakout_hi = local_game_stats.breakout_hi;
    payload->dino_hi = local_game_stats.dino_hi;
    payload->pong_hi = local_game_stats.pong_hi;
    payload->snake_hi = local_game_stats.snake_hi;
    payload->tetris_hi = local_game_stats.tetris_hi;
    payload->shooter_hi = local_game_stats.shooter_hi;
    payload->reserved[0] = local_cal.valid ? STORAGE_SCHEMA_CAL_VALID_MAGIC : 0U;
}

void storage_schema_to_runtime(const StorageSchemaPayload *payload,
                               SettingsState *settings,
                               AlarmState *alarms,
                               uint8_t count,
                               SensorCalibrationData *cal,
                               GameStatsState *game_stats)
{
    if (settings != NULL) {
        seed_default_settings(settings);
    }
    if (alarms != NULL) {
        if (count > APP_MAX_ALARMS) {
            count = APP_MAX_ALARMS;
        }
        for (uint8_t i = 0U; i < count; ++i) {
            seed_default_alarm(&alarms[i], i);
        }
    }
    if (cal != NULL) {
        memset(cal, 0, sizeof(*cal));
    }
    if (game_stats != NULL) {
        memset(game_stats, 0, sizeof(*game_stats));
    }

    if (payload == NULL) {
        return;
    }

    if (settings != NULL) {
        storage_unpack_settings0(payload->settings0, settings);
        settings->goal = payload->settings1;
        if (settings->goal < 1000U || settings->goal > 30000U) settings->goal = DEFAULT_STEP_GOAL;
        if (settings->screen_timeout_idx > 2U) settings->screen_timeout_idx = 0U;
        if (settings->sensor_sensitivity > 2U) settings->sensor_sensitivity = DEFAULT_SENSOR_SENSITIVITY;
        if (settings->brightness > 3U) settings->brightness = DEFAULT_BRIGHTNESS;
        if (settings->watchface > 2U) settings->watchface = DEFAULT_WATCHFACE;
    }

    if (alarms != NULL) {
        if (count > 0U) storage_unpack_alarm_word(payload->alarms[0], (uint8_t)(payload->alarm_meta & 0x0FU), &alarms[0], 0U);
        if (count > 1U) storage_unpack_alarm_word(payload->alarms[1], (uint8_t)((payload->alarm_meta >> 4) & 0x0FU), &alarms[1], 1U);
        if (count > 2U) storage_unpack_alarm_word(payload->alarms[2], (uint8_t)((payload->alarm_meta >> 8) & 0x0FU), &alarms[2], 2U);
    }

    if (cal != NULL) {
        cal->valid = payload->reserved[0] == STORAGE_SCHEMA_CAL_VALID_MAGIC;
        if (cal->valid) {
            cal->az_bias = payload->cal[0];
            cal->gx_bias = payload->cal[1];
            cal->gy_bias = payload->cal[2];
            cal->gz_bias = payload->cal[3];
        }
    }

    if (game_stats != NULL) {
        game_stats->breakout_hi = payload->breakout_hi;
        game_stats->dino_hi = payload->dino_hi;
        game_stats->pong_hi = payload->pong_hi;
        game_stats->snake_hi = payload->snake_hi;
        game_stats->tetris_hi = payload->tetris_hi;
        game_stats->shooter_hi = payload->shooter_hi;
    }
}

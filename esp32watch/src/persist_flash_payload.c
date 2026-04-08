#include "persist_flash_internal.h"
#include "app_config.h"
#include "common/crc16.h"
#include <string.h>

void persist_flash_seed_default_settings(SettingsState *settings)
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

void persist_flash_seed_default_alarm(AlarmState *alarm, uint8_t index)
{
    memset(alarm, 0, sizeof(*alarm));
    alarm->enabled = false;
    alarm->hour = DEFAULT_ALARM_HOUR;
    alarm->minute = DEFAULT_ALARM_MINUTE;
    alarm->repeat_mask = 0x7FU;
    alarm->label_id = index;
    alarm->ringing = false;
    alarm->snooze_until_epoch = 0U;
    alarm->last_trigger_day = 0U;
}

void persist_flash_payload_from_runtime(FlashStoragePayload *payload,
                                        const SettingsState *settings,
                                        const AlarmState *alarms,
                                        const SensorCalibrationData *cal,
                                        const GameStatsState *game_stats)
{
    storage_schema_from_runtime(payload, settings, alarms, cal, game_stats);
}

void persist_flash_payload_to_settings(const FlashStoragePayload *payload, SettingsState *settings)
{
    storage_schema_to_runtime(payload, settings, NULL, 0U, NULL, NULL);
}

void persist_flash_payload_to_alarms(const FlashStoragePayload *payload, AlarmState *alarms, uint8_t count)
{
    storage_schema_to_runtime(payload, NULL, alarms, count, NULL, NULL);
}

void persist_flash_payload_to_calibration(const FlashStoragePayload *payload, SensorCalibrationData *cal)
{
    storage_schema_to_runtime(payload, NULL, NULL, 0U, cal, NULL);
}

void persist_flash_payload_to_game_stats(const FlashStoragePayload *payload, GameStatsState *game_stats)
{
    storage_schema_to_runtime(payload, NULL, NULL, 0U, NULL, game_stats);
}

bool persist_flash_validate_record(const FlashStorageRecord *rec, FlashPageInfo *info)
{
    size_t payload_len;
    uint16_t calc_crc;
    FlashStoragePayload upgraded;

    if (rec->magic != FLASH_STORAGE_MAGIC) {
        return false;
    }
    if (rec->state != FLASH_RECORD_STATE_VALID && rec->state != FLASH_RECORD_STATE_RX) {
        return false;
    }

    payload_len = storage_schema_payload_size_for_version((uint8_t)rec->version);
    if (payload_len == 0U || rec->length != payload_len) {
        return false;
    }

    info->stored_crc = rec->crc;
    calc_crc = crc16_buf((const uint8_t *)&rec->payload, payload_len);
    info->calc_crc = calc_crc;
    if (info->stored_crc != calc_crc) {
        return false;
    }
    if (!storage_schema_upgrade_payload((uint8_t)rec->version, &rec->payload, payload_len, &upgraded)) {
        return false;
    }

    info->sequence = rec->sequence;
    info->payload = upgraded;
    info->valid = rec->state == FLASH_RECORD_STATE_VALID;
    info->receive = rec->state == FLASH_RECORD_STATE_RX;
    return true;
}

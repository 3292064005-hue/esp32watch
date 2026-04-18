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

namespace {
constexpr uint16_t PERSIST_CAL_VALID_FLAG = 0x1000U;
constexpr const char *kGameStatsVersionKey = "version";
constexpr const char *kGameBreakoutKey = "breakout";
constexpr const char *kGameDinoKey = "dino";
constexpr const char *kGamePongKey = "pong";
constexpr const char *kGameSnakeKey = "snake";
constexpr const char *kGameTetrisKey = "tetris";
constexpr const char *kGameShooterKey = "shooter";

constexpr const char *kSettingsRecordKey = "settings";
constexpr const char *kAlarmsRecordKey = "alarms";
constexpr const char *kCalibrationRecordKey = "calibration";
constexpr uint32_t kSettingsRecordMagic = 0x53455453UL;     // SETS
constexpr uint32_t kAlarmsRecordMagic = 0x414C524DUL;       // ALRM
constexpr uint32_t kCalibrationRecordMagic = 0x43414C42UL;  // CALB

struct DurableSettingsRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t crc;
    uint16_t settings0;
    uint16_t settings1;
    uint16_t settings2;
};

struct DurableAlarmsRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t crc;
    uint16_t alarm0;
    uint16_t alarm1;
    uint16_t alarm2;
    uint16_t alarm_meta;
};

struct DurableCalibrationRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t crc;
    uint8_t valid;
    int16_t az_bias;
    int16_t gx_bias;
    int16_t gy_bias;
    int16_t gz_bias;
};

Preferences g_game_stats_prefs;
Preferences g_settings_prefs;
Preferences g_alarms_prefs;
Preferences g_calibration_prefs;

static bool persist_game_stats_open(bool read_only)
{
    return persist_preferences_begin(g_game_stats_prefs, PERSIST_PREFS_DOMAIN_GAME_STATS, read_only);
}

static bool persist_settings_open(bool read_only)
{
    return persist_preferences_begin(g_settings_prefs, PERSIST_PREFS_DOMAIN_APP_SETTINGS, read_only);
}

static bool persist_alarms_open(bool read_only)
{
    return persist_preferences_begin(g_alarms_prefs, PERSIST_PREFS_DOMAIN_APP_ALARMS, read_only);
}

static bool persist_calibration_open(bool read_only)
{
    return persist_preferences_begin(g_calibration_prefs, PERSIST_PREFS_DOMAIN_SENSOR_CALIBRATION, read_only);
}

static uint16_t crc16_mix_local(uint16_t crc, uint16_t value)
{
    crc ^= value;
    for (uint8_t bit = 0; bit < 16U; ++bit) {
        crc = (crc & 1U) ? (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
    }
    return crc;
}

static uint16_t calc_crc(uint16_t settings0, uint16_t settings1, uint16_t settings2,
                         uint16_t alarm0, uint16_t alarm1, uint16_t alarm2, uint16_t alarm_meta,
                         uint16_t cal0, uint16_t cal1, uint16_t cal2, uint16_t cal3)
{
    uint16_t crc = 0xFFFFU;
    crc = crc16_mix_local(crc, settings0);
    crc = crc16_mix_local(crc, settings1);
    crc = crc16_mix_local(crc, settings2);
    crc = crc16_mix_local(crc, alarm0);
    crc = crc16_mix_local(crc, alarm1);
    crc = crc16_mix_local(crc, alarm2);
    crc = crc16_mix_local(crc, alarm_meta);
    crc = crc16_mix_local(crc, cal0);
    crc = crc16_mix_local(crc, cal1);
    crc = crc16_mix_local(crc, cal2);
    crc = crc16_mix_local(crc, cal3);
    return crc;
}

static void read_raw(uint16_t *settings0, uint16_t *settings1, uint16_t *settings2,
                     uint16_t *alarm0, uint16_t *alarm1, uint16_t *alarm2, uint16_t *alarm_meta,
                     uint16_t *cal0, uint16_t *cal1, uint16_t *cal2, uint16_t *cal3)
{
    *settings0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS0);
    *settings1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS1);
    *settings2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS2);
    *alarm0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM);
    *alarm1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM1);
    *alarm2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM2);
    *alarm_meta = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META);
    *cal0 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL0);
    *cal1 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL1);
    *cal2 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL2);
    *cal3 = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL3);
}

static void write_crc(void)
{
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_CRC, persist_get_calculated_crc());
}

static bool durable_app_state_enabled(void)
{
#if APP_STORAGE_USE_PREFERENCES_APP_STATE
    return true;
#else
    return false;
#endif
}

static bool durable_record_valid(const DurableSettingsRecord &record)
{
    DurableSettingsRecord normalized = record;
    normalized.crc = 0U;
    return record.magic == kSettingsRecordMagic &&
           record.version == APP_STORAGE_VERSION &&
           record.crc == calc_crc(record.settings0,
                                  record.settings1,
                                  record.settings2,
                                  0U,
                                  0U,
                                  0U,
                                  0U,
                                  0U,
                                  0U,
                                  0U,
                                  0U);
}

static bool durable_record_valid(const DurableAlarmsRecord &record)
{
    return record.magic == kAlarmsRecordMagic &&
           record.version == APP_STORAGE_VERSION &&
           record.crc == calc_crc(0U,
                                  0U,
                                  0U,
                                  record.alarm0,
                                  record.alarm1,
                                  record.alarm2,
                                  record.alarm_meta,
                                  0U,
                                  0U,
                                  0U,
                                  0U);
}

static bool durable_record_valid(const DurableCalibrationRecord &record)
{
    const uint16_t cal_valid = record.valid ? PERSIST_CAL_VALID_FLAG : 0U;
    return record.magic == kCalibrationRecordMagic &&
           record.version == APP_STORAGE_VERSION &&
           record.crc == calc_crc(0U,
                                  0U,
                                  0U,
                                  0U,
                                  0U,
                                  0U,
                                  cal_valid,
                                  (uint16_t)record.az_bias,
                                  (uint16_t)record.gx_bias,
                                  (uint16_t)record.gy_bias,
                                  (uint16_t)record.gz_bias);
}

static DurableSettingsRecord durable_build_settings_record(const SettingsState &settings)
{
    DurableSettingsRecord record = {};
    record.magic = kSettingsRecordMagic;
    record.version = APP_STORAGE_VERSION;
    record.settings0 = storage_pack_settings0(&settings);
    record.settings1 = storage_pack_settings1(&settings);
    record.settings2 = storage_pack_settings2(&settings);
    record.crc = calc_crc(record.settings0,
                          record.settings1,
                          record.settings2,
                          0U,
                          0U,
                          0U,
                          0U,
                          0U,
                          0U,
                          0U,
                          0U);
    return record;
}

static DurableAlarmsRecord durable_build_alarms_record(const AlarmState *alarms, uint8_t count)
{
    AlarmState local[APP_MAX_ALARMS] = {};
    DurableAlarmsRecord record = {};
    for (uint8_t i = 0U; i < APP_MAX_ALARMS && i < count; ++i) {
        local[i] = alarms[i];
    }
    record.magic = kAlarmsRecordMagic;
    record.version = APP_STORAGE_VERSION;
    record.alarm0 = storage_pack_alarm_word(&local[0]);
    record.alarm1 = storage_pack_alarm_word(&local[1]);
    record.alarm2 = storage_pack_alarm_word(&local[2]);
    record.alarm_meta = storage_pack_alarm_meta(&local[0], &local[1], &local[2]);
    record.crc = calc_crc(0U,
                          0U,
                          0U,
                          record.alarm0,
                          record.alarm1,
                          record.alarm2,
                          record.alarm_meta,
                          0U,
                          0U,
                          0U,
                          0U);
    return record;
}

static DurableCalibrationRecord durable_build_calibration_record(const SensorCalibrationData *cal)
{
    DurableCalibrationRecord record = {};
    record.magic = kCalibrationRecordMagic;
    record.version = APP_STORAGE_VERSION;
    record.valid = cal != NULL && cal->valid;
    record.az_bias = (int16_t)((cal != NULL) ? cal->az_bias : 0);
    record.gx_bias = (int16_t)((cal != NULL) ? cal->gx_bias : 0);
    record.gy_bias = (int16_t)((cal != NULL) ? cal->gy_bias : 0);
    record.gz_bias = (int16_t)((cal != NULL) ? cal->gz_bias : 0);
    record.crc = calc_crc(0U,
                          0U,
                          0U,
                          0U,
                          0U,
                          0U,
                          record.valid ? PERSIST_CAL_VALID_FLAG : 0U,
                          (uint16_t)record.az_bias,
                          (uint16_t)record.gx_bias,
                          (uint16_t)record.gy_bias,
                          (uint16_t)record.gz_bias);
    return record;
}

static void rtc_mirror_settings(const SettingsState *settings)
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

static void rtc_mirror_alarms(const AlarmState *alarms, uint8_t count)
{
    AlarmState local[APP_MAX_ALARMS] = {};
    uint16_t meta;
    for (uint8_t i = 0U; i < APP_MAX_ALARMS && i < count; ++i) {
        local[i] = alarms[i];
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM, storage_pack_alarm_word(&local[0]));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM1, storage_pack_alarm_word(&local[1]));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM2, storage_pack_alarm_word(&local[2]));
    meta = storage_pack_alarm_meta(&local[0], &local[1], &local[2]);
    if ((platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & PERSIST_CAL_VALID_FLAG) != 0U) {
        meta |= PERSIST_CAL_VALID_FLAG;
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM_META, meta);
}

static void rtc_mirror_calibration(const SensorCalibrationData *cal)
{
    uint16_t meta = (uint16_t)(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & 0x0FFFU);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_MAGIC, BACKUP_MAGIC);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_VERSION, APP_STORAGE_VERSION);
    if (cal != NULL && cal->valid) {
        meta |= PERSIST_CAL_VALID_FLAG;
    }
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_ALARM_META, meta);
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL0, (uint16_t)((cal != NULL) ? cal->az_bias : 0));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL1, (uint16_t)((cal != NULL) ? cal->gx_bias : 0));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL2, (uint16_t)((cal != NULL) ? cal->gy_bias : 0));
    platform_rtc_backup_write(&platform_rtc_main, BKP_DR_SENSOR_CAL3, (uint16_t)((cal != NULL) ? cal->gz_bias : 0));
}

static bool durable_store_settings(const SettingsState *settings)
{
    if (!durable_app_state_enabled() || settings == NULL) {
        return false;
    }
    DurableSettingsRecord record = durable_build_settings_record(*settings);
    if (!persist_settings_open(false)) {
        return false;
    }
    const bool ok = g_settings_prefs.putBytes(kSettingsRecordKey, &record, sizeof(record)) == sizeof(record);
    g_settings_prefs.end();
    return ok;
}

static bool durable_load_settings(SettingsState *settings)
{
    DurableSettingsRecord record = {};
    if (!durable_app_state_enabled() || settings == NULL) {
        return false;
    }
    if (!persist_settings_open(true)) {
        return false;
    }
    const size_t len = g_settings_prefs.getBytesLength(kSettingsRecordKey);
    const bool ok = len == sizeof(record) &&
                    g_settings_prefs.getBytes(kSettingsRecordKey, &record, sizeof(record)) == sizeof(record) &&
                    durable_record_valid(record);
    g_settings_prefs.end();
    if (!ok) {
        return false;
    }
    memset(settings, 0, sizeof(*settings));
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
    return true;
}

static bool durable_store_alarms(const AlarmState *alarms, uint8_t count)
{
    if (!durable_app_state_enabled() || alarms == NULL) {
        return false;
    }
    DurableAlarmsRecord record = durable_build_alarms_record(alarms, count);
    if (!persist_alarms_open(false)) {
        return false;
    }
    const bool ok = g_alarms_prefs.putBytes(kAlarmsRecordKey, &record, sizeof(record)) == sizeof(record);
    g_alarms_prefs.end();
    return ok;
}

static bool durable_load_alarms(AlarmState *alarms, uint8_t count)
{
    DurableAlarmsRecord record = {};
    uint16_t meta;
    if (!durable_app_state_enabled() || alarms == NULL || count == 0U) {
        return false;
    }
    if (!persist_alarms_open(true)) {
        return false;
    }
    const size_t len = g_alarms_prefs.getBytesLength(kAlarmsRecordKey);
    const bool ok = len == sizeof(record) &&
                    g_alarms_prefs.getBytes(kAlarmsRecordKey, &record, sizeof(record)) == sizeof(record) &&
                    durable_record_valid(record);
    g_alarms_prefs.end();
    if (!ok) {
        return false;
    }
    for (uint8_t i = 0U; i < count; ++i) {
        alarms[i].enabled = false;
        alarms[i].hour = DEFAULT_ALARM_HOUR;
        alarms[i].minute = DEFAULT_ALARM_MINUTE;
        alarms[i].repeat_mask = 0x7FU;
        alarms[i].label_id = i;
        alarms[i].ringing = false;
        alarms[i].snooze_until_epoch = 0U;
        alarms[i].last_trigger_day = 0U;
    }
    meta = record.alarm_meta;
    if (count > 0U) {
        storage_unpack_alarm_word(record.alarm0, (uint8_t)(meta & 0x0FU), &alarms[0], 0U);
    }
    if (count > 1U) {
        storage_unpack_alarm_word(record.alarm1, (uint8_t)((meta >> 4) & 0x0FU), &alarms[1], 1U);
    }
    if (count > 2U) {
        storage_unpack_alarm_word(record.alarm2, (uint8_t)((meta >> 8) & 0x0FU), &alarms[2], 2U);
    }
    return true;
}

static bool durable_store_calibration(const SensorCalibrationData *cal)
{
    if (!durable_app_state_enabled()) {
        return false;
    }
    DurableCalibrationRecord record = durable_build_calibration_record(cal);
    if (!persist_calibration_open(false)) {
        return false;
    }
    const bool ok = g_calibration_prefs.putBytes(kCalibrationRecordKey, &record, sizeof(record)) == sizeof(record);
    g_calibration_prefs.end();
    return ok;
}

static bool durable_load_calibration(SensorCalibrationData *cal)
{
    DurableCalibrationRecord record = {};
    if (!durable_app_state_enabled() || cal == NULL) {
        return false;
    }
    if (!persist_calibration_open(true)) {
        return false;
    }
    const size_t len = g_calibration_prefs.getBytesLength(kCalibrationRecordKey);
    const bool ok = len == sizeof(record) &&
                    g_calibration_prefs.getBytes(kCalibrationRecordKey, &record, sizeof(record)) == sizeof(record) &&
                    durable_record_valid(record);
    g_calibration_prefs.end();
    if (!ok) {
        return false;
    }
    cal->valid = record.valid;
    cal->az_bias = record.az_bias;
    cal->gx_bias = record.gx_bias;
    cal->gy_bias = record.gy_bias;
    cal->gz_bias = record.gz_bias;
    return true;
}

static bool durable_app_state_backend_ready_internal(void)
{
    Preferences probe;
    if (!durable_app_state_enabled()) {
        return false;
    }

    /*
     * Open the namespaces read-write so first boot after a firmware upgrade can
     * create the durable app-state domains before attempting RTC->NVS migration.
     * A read-only open would report "not ready" on a fresh namespace and leave
     * upgraded RTC state stranded in reset-domain storage until a later save.
     */
    if (!persist_preferences_begin(probe, PERSIST_PREFS_DOMAIN_APP_SETTINGS, false)) {
        return false;
    }
    probe.end();
    if (!persist_preferences_begin(probe, PERSIST_PREFS_DOMAIN_APP_ALARMS, false)) {
        return false;
    }
    probe.end();
    if (!persist_preferences_begin(probe, PERSIST_PREFS_DOMAIN_SENSOR_CALIBRATION, false)) {
        return false;
    }
    probe.end();
    return true;
}

static void durable_migrate_or_mirror(void)
{
    if (!durable_app_state_backend_ready_internal()) {
        return;
    }

    SettingsState settings = {};
    AlarmState alarms[APP_MAX_ALARMS] = {};
    SensorCalibrationData calibration = {};
    const bool has_settings = durable_load_settings(&settings);
    const bool has_alarms = durable_load_alarms(alarms, APP_MAX_ALARMS);
    const bool has_cal = durable_load_calibration(&calibration);
    const bool durable_complete = has_settings && has_alarms && has_cal;

    if (durable_complete) {
        rtc_mirror_settings(&settings);
        rtc_mirror_alarms(alarms, APP_MAX_ALARMS);
        rtc_mirror_calibration(&calibration);
        write_crc();
        return;
    }

    if (!persist_is_initialized()) {
        return;
    }

    persist_load_settings(&settings);
    persist_load_alarms(alarms, APP_MAX_ALARMS);
    persist_load_sensor_calibration(&calibration);
    (void)durable_store_settings(&settings);
    (void)durable_store_alarms(alarms, APP_MAX_ALARMS);
    (void)durable_store_calibration(&calibration);
    rtc_mirror_settings(&settings);
    rtc_mirror_alarms(alarms, APP_MAX_ALARMS);
    rtc_mirror_calibration(&calibration);
    write_crc();
}
} // namespace

extern "C" void persist_init(void)
{
    platform_rtc_backup_unlock();
    durable_migrate_or_mirror();
}

extern "C" bool persist_app_state_durable_ready(void)
{
    return durable_app_state_backend_ready_internal();
}

extern "C" const char *persist_app_state_backend_name(void)
{
    return persist_app_state_durable_ready() ? "NVS_APP_STATE" : "RTC_RESET_DOMAIN";
}

extern "C" uint8_t persist_get_version(void)
{
    return (uint8_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_VERSION);
}

extern "C" uint16_t persist_get_stored_crc(void)
{
    return (uint16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_CRC);
}

extern "C" uint16_t persist_get_calculated_crc(void)
{
    uint16_t settings0, settings1, settings2, alarm0, alarm1, alarm2, alarm_meta, cal0, cal1, cal2, cal3;
    read_raw(&settings0, &settings1, &settings2, &alarm0, &alarm1, &alarm2, &alarm_meta, &cal0, &cal1, &cal2, &cal3);
    return calc_crc(settings0, settings1, settings2, alarm0, alarm1, alarm2, alarm_meta, cal0, cal1, cal2, cal3);
}

extern "C" bool persist_is_initialized(void)
{
    if (platform_rtc_backup_read(&platform_rtc_main, BKP_DR_MAGIC) != BACKUP_MAGIC) {
        return false;
    }
    if (persist_get_version() != APP_STORAGE_VERSION) {
        return false;
    }
    return persist_get_stored_crc() == persist_get_calculated_crc();
}

/**
 * @brief Persist authoritative settings to the durable app-state backend and mirror them into RTC backup storage.
 *
 * @param[in] settings Normalized settings snapshot to persist.
 * @return void
 * @throws None.
 * @boundary_behavior When durable Preferences storage is unavailable, the function still updates RTC backup storage so reset-domain behavior remains intact.
 */
extern "C" void persist_save_settings(const SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
    (void)durable_store_settings(settings);
    rtc_mirror_settings(settings);
    write_crc();
}

/**
 * @brief Load authoritative settings from durable app-state storage when available, otherwise from RTC backup storage.
 *
 * @param[out] settings Destination settings snapshot.
 * @return void
 * @throws None.
 * @boundary_behavior Falls back to the reset-domain mirror when durable storage is unavailable or the durable record is invalid, while preserving legacy range clamping.
 */
extern "C" void persist_load_settings(SettingsState *settings)
{
    if (settings == NULL) {
        return;
    }
    if (durable_load_settings(settings)) {
        return;
    }
    if (!persist_is_initialized()) {
        return;
    }
    storage_unpack_settings0(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS0), settings);
    settings->goal = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SETTINGS1);
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

/**
 * @brief Persist the full alarm set to the durable app-state backend and mirror it into RTC backup storage.
 *
 * @param[in] alarms Source alarm array.
 * @param[in] count Number of entries in @p alarms.
 * @return void
 * @throws None.
 * @boundary_behavior Writes all alarm slots through a single durable record so power-loss recovery and RTC mirror semantics remain aligned.
 */
extern "C" void persist_save_alarms(const AlarmState *alarms, uint8_t count)
{
    if (alarms == NULL) {
        return;
    }
    (void)durable_store_alarms(alarms, count);
    rtc_mirror_alarms(alarms, count);
    write_crc();
}

/**
 * @brief Load the alarm set from durable app-state storage when available, otherwise from RTC backup storage.
 *
 * @param[out] alarms Destination alarm array.
 * @param[in] count Number of entries requested.
 * @return void
 * @throws None.
 * @boundary_behavior Initializes all requested alarm slots to defaults before applying persisted fields so callers never observe uninitialized entries.
 */
extern "C" void persist_load_alarms(AlarmState *alarms, uint8_t count)
{
    uint16_t meta;
    if (alarms == NULL || count == 0U) {
        return;
    }
    if (durable_load_alarms(alarms, count)) {
        return;
    }
    for (uint8_t i = 0; i < count; ++i) {
        alarms[i].enabled = false;
        alarms[i].hour = DEFAULT_ALARM_HOUR;
        alarms[i].minute = DEFAULT_ALARM_MINUTE;
        alarms[i].repeat_mask = 0x7FU;
        alarms[i].label_id = i;
        alarms[i].ringing = false;
        alarms[i].snooze_until_epoch = 0U;
        alarms[i].last_trigger_day = 0U;
    }
    if (!persist_is_initialized()) {
        return;
    }
    meta = platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META);
    if (count > 0U) {
        storage_unpack_alarm_word(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM), (uint8_t)(meta & 0x0FU), &alarms[0], 0U);
    }
    if (count > 1U) {
        storage_unpack_alarm_word(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM1), (uint8_t)((meta >> 4) & 0x0FU), &alarms[1], 1U);
    }
    if (count > 2U) {
        storage_unpack_alarm_word(platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM2), (uint8_t)((meta >> 8) & 0x0FU), &alarms[2], 2U);
    }
}

extern "C" void persist_save_game_stats(const GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }
    if (!persist_game_stats_open(false)) {
        return;
    }
    g_game_stats_prefs.putUChar(kGameStatsVersionKey, APP_STORAGE_VERSION);
    g_game_stats_prefs.putULong(kGameBreakoutKey, stats->breakout_hi);
    g_game_stats_prefs.putULong(kGameDinoKey, stats->dino_hi);
    g_game_stats_prefs.putULong(kGamePongKey, stats->pong_hi);
    g_game_stats_prefs.putULong(kGameSnakeKey, stats->snake_hi);
    g_game_stats_prefs.putULong(kGameTetrisKey, stats->tetris_hi);
    g_game_stats_prefs.putULong(kGameShooterKey, stats->shooter_hi);
    g_game_stats_prefs.end();
}

extern "C" void persist_load_game_stats(GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    if (!persist_game_stats_open(true)) {
        return;
    }
    if (g_game_stats_prefs.getUChar(kGameStatsVersionKey, 0U) != APP_STORAGE_VERSION) {
        g_game_stats_prefs.end();
        return;
    }
    stats->breakout_hi = g_game_stats_prefs.getULong(kGameBreakoutKey, 0U);
    stats->dino_hi = g_game_stats_prefs.getULong(kGameDinoKey, 0U);
    stats->pong_hi = g_game_stats_prefs.getULong(kGamePongKey, 0U);
    stats->snake_hi = g_game_stats_prefs.getULong(kGameSnakeKey, 0U);
    stats->tetris_hi = g_game_stats_prefs.getULong(kGameTetrisKey, 0U);
    stats->shooter_hi = g_game_stats_prefs.getULong(kGameShooterKey, 0U);
    g_game_stats_prefs.end();
}

/**
 * @brief Persist sensor calibration to the durable app-state backend and mirror it into RTC backup storage.
 *
 * @param[in] cal Optional calibration payload. Pass NULL to clear the durable record and RTC mirror.
 * @return void
 * @throws None.
 * @boundary_behavior Persists the calibration-valid bit together with the bias payload so durable recovery and reset-domain recovery remain consistent.
 */
extern "C" void persist_save_sensor_calibration(const SensorCalibrationData *cal)
{
    (void)durable_store_calibration(cal);
    rtc_mirror_calibration(cal);
    write_crc();
}

/**
 * @brief Load sensor calibration from durable app-state storage when available, otherwise from RTC backup storage.
 *
 * @param[out] cal Destination calibration structure.
 * @return void
 * @throws None.
 * @boundary_behavior Returns a cleared, invalid calibration when no durable or reset-domain record is available.
 */
extern "C" void persist_load_sensor_calibration(SensorCalibrationData *cal)
{
    if (cal == NULL) {
        return;
    }
    cal->valid = false;
    cal->az_bias = 0;
    cal->gx_bias = 0;
    cal->gy_bias = 0;
    cal->gz_bias = 0;
    if (durable_load_calibration(cal)) {
        return;
    }
    if (!persist_is_initialized()) {
        return;
    }
    cal->valid = (platform_rtc_backup_read(&platform_rtc_main, BKP_DR_ALARM_META) & PERSIST_CAL_VALID_FLAG) != 0U;
    if (!cal->valid) {
        return;
    }
    cal->az_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL0);
    cal->gx_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL1);
    cal->gy_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL2);
    cal->gz_bias = (int16_t)platform_rtc_backup_read(&platform_rtc_main, BKP_DR_SENSOR_CAL3);
}

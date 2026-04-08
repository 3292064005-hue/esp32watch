#include "model_internal.h"
#include "app_config.h"
#include "app_tuning.h"
#include "services/storage_service.h"
#include "services/time_service.h"
#include "services/diag_service.h"
#include "reset_reason.h"
#include "platform_api.h"
#include <string.h>

WatchModel g_model;
static uint32_t g_last_rtc_refresh;
static uint32_t g_last_low_bat_popup;
static uint32_t g_last_active_minute_ms;

typedef struct {
    uint32_t flags;
    StorageCommitReason commit_reason;
} ModelRuntimeRequestState;

static ModelRuntimeRequestState g_model_runtime_requests;

static uint8_t model_commit_reason_priority(StorageCommitReason reason)
{
    switch (reason) {
        case STORAGE_COMMIT_REASON_RESTORE_DEFAULTS: return 5U;
        case STORAGE_COMMIT_REASON_SLEEP: return 4U;
        case STORAGE_COMMIT_REASON_ALARM: return 3U;
        case STORAGE_COMMIT_REASON_MANUAL: return 2U;
        case STORAGE_COMMIT_REASON_CALIBRATION: return 2U;
        case STORAGE_COMMIT_REASON_MAX_AGE: return 1U;
        case STORAGE_COMMIT_REASON_IDLE: return 1U;
        default: return 0U;
    }
}

static void model_set_time_state(TimeState state)
{
    g_model.time_state = state;
    g_model.time_valid = state != TIME_STATE_UNSET;
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN);
}

static void popup_queue_sync_visible(void)
{
    if (g_model.popup_queue_count > 0U) {
        g_model.popup = g_model.popup_queue[0];
        g_model.popup_latched = true;
    } else {
        g_model.popup = POPUP_NONE;
        g_model.popup_latched = false;
    }
}

bool model_popup_queue_contains(PopupType popup)
{
    if (popup == POPUP_NONE) {
        return false;
    }
    for (uint8_t i = 0U; i < g_model.popup_queue_count; ++i) {
        if (g_model.popup_queue[i] == popup) {
            return true;
        }
    }
    return false;
}

void model_internal_clear_popups(void)
{
    memset(g_model.popup_queue, 0, sizeof(g_model.popup_queue));
    g_model.popup_queue_count = 0U;
    popup_queue_sync_visible();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_UI);
}

void model_push_popup(PopupType popup, bool priority)
{
    uint8_t insert_at;

    if (popup == POPUP_NONE || model_popup_queue_contains(popup)) {
        popup_queue_sync_visible();
        return;
    }

    if (g_model.popup_queue_count >= APP_POPUP_QUEUE_DEPTH) {
        if (!priority) {
            popup_queue_sync_visible();
            return;
        }
        g_model.popup_queue_count = (uint8_t)(APP_POPUP_QUEUE_DEPTH - 1U);
    }

    insert_at = priority ? 0U : g_model.popup_queue_count;
    for (uint8_t i = g_model.popup_queue_count; i > insert_at; --i) {
        g_model.popup_queue[i] = g_model.popup_queue[i - 1U];
    }
    g_model.popup_queue[insert_at] = popup;
    if (g_model.popup_queue_count < APP_POPUP_QUEUE_DEPTH) {
        g_model.popup_queue_count++;
    }
    popup_queue_sync_visible();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_UI);
}

void model_internal_pop_popup(void)
{
    if (g_model.popup_queue_count > 0U) {
        for (uint8_t i = 1U; i < g_model.popup_queue_count; ++i) {
            g_model.popup_queue[i - 1U] = g_model.popup_queue[i];
        }
        g_model.popup_queue[g_model.popup_queue_count - 1U] = POPUP_NONE;
        g_model.popup_queue_count--;
    }
    popup_queue_sync_visible();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_UI);
}

void model_internal_request_runtime_sync(uint32_t flags, StorageCommitReason reason)
{
    g_model_runtime_requests.flags |= flags;
    if (model_commit_reason_priority(reason) >= model_commit_reason_priority(g_model_runtime_requests.commit_reason)) {
        g_model_runtime_requests.commit_reason = reason;
    }
    model_internal_mark_projection_dirty(MODEL_PROJECTION_DIRTY_UI);
}

void model_internal_persist_settings(void)
{
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STAGE_SETTINGS, STORAGE_COMMIT_REASON_NONE);
}

void model_internal_persist_all_alarms(void)
{
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STAGE_ALARMS, STORAGE_COMMIT_REASON_NONE);
}

void model_internal_persist_game_stats(void)
{
    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STAGE_GAME_STATS |
                                            MODEL_RUNTIME_REQUEST_STORAGE_COMMIT,
                                        STORAGE_COMMIT_REASON_IDLE);
}

static void set_alarm_defaults(AlarmState *alarm, uint8_t index)
{
    memset(alarm, 0, sizeof(*alarm));
    alarm->enabled = (index == 0U);
    alarm->hour = (uint8_t)((DEFAULT_ALARM_HOUR + index) % 24U);
    alarm->minute = DEFAULT_ALARM_MINUTE;
    alarm->repeat_mask = 0x7FU;
    alarm->label_id = index;
}

uint32_t model_internal_timeout_idx_to_ms(uint8_t idx)
{
    switch (idx) {
        case 1U: return SCREEN_SLEEP_LONG_MS;
        case 2U: return SCREEN_SLEEP_MAX_MS;
        default: return SCREEN_SLEEP_DEFAULT_MS;
    }
}

static void apply_defaults(SettingsState *settings, AlarmState *alarms)
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

    for (uint8_t i = 0; i < APP_MAX_ALARMS; ++i) {
        set_alarm_defaults(&alarms[i], i);
    }
}

static void seed_unset_datetime(void)
{
    g_model.now.year = 2024U;
    g_model.now.month = 1U;
    g_model.now.day = 1U;
    g_model.now.weekday = 1U;
    g_model.now.hour = 0U;
    g_model.now.minute = 0U;
    g_model.now.second = 0U;
}

static void default_datetime_if_invalid(void)
{
    uint32_t epoch = time_service_get_epoch();
    if (!time_service_is_reasonable_epoch(epoch)) {
        diag_service_note_rtc_invalid();
        model_set_time_state(TIME_STATE_UNSET);
        seed_unset_datetime();
        time_service_note_recovery_epoch(0U, false);
        return;
    }
    model_set_time_state(TIME_STATE_VALID);
}

static void refresh_time_from_rtc(void)
{
    uint32_t epoch = time_service_get_epoch();
    if (!time_service_is_reasonable_epoch(epoch)) {
        diag_service_note_rtc_invalid();
        if (!g_model.time_valid) {
            seed_unset_datetime();
            time_service_note_recovery_epoch(0U, false);
        }
        model_set_time_state(TIME_STATE_UNSET);
        return;
    }
    time_service_epoch_to_datetime(epoch, &g_model.now);
    if (g_model.time_state != TIME_STATE_RECOVERED) {
        model_set_time_state(TIME_STATE_VALID);
    } else {
        g_model.time_valid = true;
    }
}

static void clear_alarm_runtime(void)
{
    for (uint8_t i = 0; i < APP_MAX_ALARMS; ++i) {
        g_model.alarms[i].ringing = false;
        g_model.alarms[i].snooze_until_epoch = 0U;
    }
    g_model.alarm_ringing_index = 0xFFU;
    model_internal_sync_selected_alarm_view();
}

static void update_day_rollover(void)
{
    uint16_t day_id;

    if (!g_model.time_valid) {
        return;
    }

    day_id = time_service_day_id_from_epoch(time_service_get_epoch());
    if (g_model.current_day_id == 0U) {
        g_model.current_day_id = day_id;
        return;
    }
    if (day_id != g_model.current_day_id) {
        g_model.current_day_id = day_id;
        g_model.activity.steps = 0U;
        g_model.activity.active_minutes = 0U;
        g_model.activity.goal_reached_today = false;
        g_last_active_minute_ms = 0U;
        clear_alarm_runtime();
    }
}

void model_init(void)
{
    SettingsState defaults;
    AlarmState alarm_defaults[APP_MAX_ALARMS];

    memset(&g_model, 0, sizeof(g_model));
    g_model.alarm_ringing_index = 0xFFU;
    apply_defaults(&defaults, alarm_defaults);

    g_model.settings = defaults;
    memcpy(g_model.alarms, alarm_defaults, sizeof(g_model.alarms));
    storage_service_load_settings(&g_model.settings);
    storage_service_load_alarms(g_model.alarms, APP_MAX_ALARMS);
    storage_service_load_game_stats(&g_model.game_stats);
    g_model.alarm_selected = 0U;
    model_internal_sync_selected_alarm_view();

    g_model.activity.goal = g_model.settings.goal;
    g_model.timer.preset_s = DEFAULT_TIMER_SECONDS;
    g_model.timer.remain_s = DEFAULT_TIMER_SECONDS;
    g_model.battery_present = false;
    g_model.battery_percent = 0U;
    g_model.screen_timeout_ms = model_internal_timeout_idx_to_ms(g_model.settings.screen_timeout_idx);
    g_model.reset_reason = reset_reason_get();
    g_model.last_wake_reason = WAKE_REASON_BOOT;
    g_model.last_sleep_reason = SLEEP_REASON_NONE;
    model_set_time_state(TIME_STATE_VALID);
    default_datetime_if_invalid();
    refresh_time_from_rtc();
    g_model.current_day_id = g_model.time_valid ? time_service_day_id_from_epoch(time_service_get_epoch()) : 0U;
    g_model.last_user_activity_ms = platform_time_now_ms();
    model_internal_sync_storage_runtime();
    model_internal_sync_power_runtime();
    model_internal_sync_input_runtime();

    if (!g_model.storage_ok) {
        storage_service_save_settings(&g_model.settings);
        storage_service_save_alarms(g_model.alarms, APP_MAX_ALARMS);
        storage_service_save_game_stats(&g_model.game_stats);
        storage_service_commit_now();
        model_internal_sync_storage_runtime();
    }

    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_ALL);
    model_internal_flush_read_models();
}

WatchModel *model_get(void)
{
    return &g_model;
}

bool model_has_runtime_requests(void)
{
    return g_model_runtime_requests.flags != 0U;
}

uint32_t model_consume_runtime_requests(StorageCommitReason *commit_reason)
{
    uint32_t flags = g_model_runtime_requests.flags;

    if (commit_reason != NULL) {
        *commit_reason = g_model_runtime_requests.commit_reason;
    }
    g_model_runtime_requests.flags = 0U;
    g_model_runtime_requests.commit_reason = STORAGE_COMMIT_REASON_NONE;
    model_internal_mark_projection_dirty(MODEL_PROJECTION_DIRTY_UI);
    return flags;
}

void model_restore_defaults(void)
{
    SettingsState defaults;
    AlarmState alarm_defaults[APP_MAX_ALARMS];

    apply_defaults(&defaults, alarm_defaults);
    g_model.settings = defaults;
    memcpy(g_model.alarms, alarm_defaults, sizeof(g_model.alarms));
    g_model.alarm_selected = 0U;
    g_model.activity.goal = g_model.settings.goal;
    g_model.activity.goal_reached_today = false;
    g_model.screen_timeout_ms = model_internal_timeout_idx_to_ms(g_model.settings.screen_timeout_idx);
    model_internal_clear_popups();
    g_model.sensor_fault_latched = false;
    g_model.sensor.calibrated = false;
    g_model.sensor.calibration_pending = true;
    g_model.sensor.calibration_progress = 0U;
    g_model.sensor.offline_since_ms = 0U;
    clear_alarm_runtime();
    g_model.stopwatch.running = false;
    g_model.stopwatch.elapsed_ms = 0U;
    g_model.stopwatch.lap_count = 0U;
    g_model.timer.running = false;
    g_model.timer.preset_s = DEFAULT_TIMER_SECONDS;
    g_model.timer.remain_s = DEFAULT_TIMER_SECONDS;
    model_internal_sync_selected_alarm_view();

    model_internal_request_runtime_sync(MODEL_RUNTIME_REQUEST_STAGE_SETTINGS |
                                            MODEL_RUNTIME_REQUEST_STAGE_ALARMS |
                                            MODEL_RUNTIME_REQUEST_STORAGE_COMMIT |
                                            MODEL_RUNTIME_REQUEST_APPLY_BRIGHTNESS |
                                            MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS |
                                            MODEL_RUNTIME_REQUEST_CLEAR_SENSOR_CALIBRATION,
                                        STORAGE_COMMIT_REASON_RESTORE_DEFAULTS);
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_RUNTIME | MODEL_PROJECTION_DIRTY_UI);
}

void model_sync_runtime_observability(void)
{
    model_internal_sync_storage_runtime();
    model_internal_sync_power_runtime();
    model_internal_sync_input_runtime();
    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_RUNTIME);
}

void model_tick(uint32_t now_ms)
{
    if ((now_ms - g_last_rtc_refresh) >= app_tuning_manifest_get()->rtc_refresh_ms) {
        g_last_rtc_refresh = now_ms;
        refresh_time_from_rtc();
        update_day_rollover();
    }

    if (g_model.stopwatch.running) {
        uint32_t delta = now_ms - g_model.stopwatch.last_tick_ms;
        g_model.stopwatch.elapsed_ms += delta;
        g_model.stopwatch.last_tick_ms = now_ms;
    }

    if (g_model.timer.running) {
        uint32_t delta = now_ms - g_model.timer.last_tick_ms;
        if (delta >= 1000U) {
            uint32_t dec = delta / 1000U;
            if (dec >= g_model.timer.remain_s) {
                g_model.timer.remain_s = 0U;
                g_model.timer.running = false;
                model_push_popup(POPUP_TIMER_DONE, false);
            } else {
                g_model.timer.remain_s -= dec;
                g_model.timer.last_tick_ms += dec * 1000U;
            }
        }
    }

    if (g_model.battery_present && g_model.battery_percent <= app_tuning_manifest_get()->low_battery_threshold) {
        if ((now_ms - g_last_low_bat_popup) > 60000U) {
            g_last_low_bat_popup = now_ms;
            model_push_popup(POPUP_LOW_BATTERY, false);
        }
    }

    if (!g_model.activity.goal_reached_today && g_model.activity.steps >= g_model.activity.goal && g_model.activity.goal > 0U) {
        g_model.activity.goal_reached_today = true;
        model_push_popup(POPUP_GOAL_REACHED, false);
    }

    if (!g_model.sensor.online && g_model.sensor.offline_since_ms != 0U) {
        if ((now_ms - g_model.sensor.offline_since_ms) >= app_tuning_manifest_get()->sensor_fault_popup_ms && !g_model.sensor_fault_latched) {
            model_push_popup(POPUP_SENSOR_FAULT, false);
            g_model.sensor_fault_latched = true;
        }
    } else if (g_model.sensor.online) {
        g_model.sensor_fault_latched = false;
    }

    if (g_model.activity.motion_state == MOTION_STATE_WALKING || g_model.activity.motion_state == MOTION_STATE_ACTIVE) {
        if (g_last_active_minute_ms == 0U) {
            g_last_active_minute_ms = now_ms;
        }
        while ((now_ms - g_last_active_minute_ms) >= 60000U) {
            g_last_active_minute_ms += 60000U;
            if (g_model.activity.active_minutes < 1440U) {
                g_model.activity.active_minutes++;
            }
        }
    } else {
        g_last_active_minute_ms = 0U;
    }

    model_internal_commit_legacy_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_UI);
}

const char *model_reset_reason_name(ResetReason reason)
{
    return reset_reason_name(reason);
}

const char *model_wake_reason_name(WakeReason reason)
{
    switch (reason) {
        case WAKE_REASON_BOOT: return "BOOT";
        case WAKE_REASON_KEY: return "KEY";
        case WAKE_REASON_POPUP: return "POP";
        case WAKE_REASON_WRIST_RAISE: return "WRST";
        case WAKE_REASON_MANUAL: return "MAN";
        case WAKE_REASON_SERVICE: return "SRV";
        default: return "NONE";
    }
}

const char *model_sleep_reason_name(SleepReason reason)
{
    switch (reason) {
        case SLEEP_REASON_AUTO_TIMEOUT: return "AUTO";
        case SLEEP_REASON_MANUAL: return "MAN";
        case SLEEP_REASON_SERVICE: return "SRV";
        default: return "NONE";
    }
}

const char *model_storage_commit_reason_name(StorageCommitReason reason)
{
    switch (reason) {
        case STORAGE_COMMIT_REASON_IDLE: return "IDLE";
        case STORAGE_COMMIT_REASON_MAX_AGE: return "MAX";
        case STORAGE_COMMIT_REASON_CALIBRATION: return "CAL";
        case STORAGE_COMMIT_REASON_MANUAL: return "MAN";
        case STORAGE_COMMIT_REASON_SLEEP: return "SLP";
        case STORAGE_COMMIT_REASON_RESTORE_DEFAULTS: return "RST";
        case STORAGE_COMMIT_REASON_ALARM: return "ALRM";
        default: return "NONE";
    }
}

const char *model_time_state_name(TimeState state)
{
    switch (state) {
        case TIME_STATE_VALID: return "VALID";
        case TIME_STATE_RECOVERED: return "RECOV";
        default: return "UNSET";
    }
}

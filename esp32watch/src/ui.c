#include "ui.h"
#include "ui_internal.h"
#include "app_config.h"
#include "app_tuning.h"
#include "services/display_service.h"
#include "display.h"
#include "model.h"
#include "services/diag_service.h"
#include "services/power_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/network_sync_service.h"
#include "services/time_service.h"
#include "services/wdt_service.h"
#include "services/input_service.h"
#include "web/web_wifi.h"
#include "platform_api.h"
#include "vibe.h"
#include "ui_snapshot.h"
#include <stdio.h>
#include <string.h>

UiState g_ui;
static uint32_t g_ui_pending_actions;
static UiModelMutation g_ui_model_mutation_queue[UI_MODEL_MUTATION_QUEUE_CAPACITY];
static uint8_t g_ui_model_mutation_head;
static uint8_t g_ui_model_mutation_tail;
static uint8_t g_ui_model_mutation_count;
static const char * const g_weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static bool g_ui_initialized;

#define UI_LAYOUT_HEADER_LINE_Y       11
#define UI_LAYOUT_FOOTER_LINE_Y       55
#define UI_LAYOUT_FOOTER_TEXT_Y       57

static int16_t ui_anim_eased_shift(uint32_t elapsed_ms, uint32_t duration_ms)
{
    float t;
    float eased;
    int32_t shift;

    if (duration_ms == 0U) {
        return OLED_WIDTH;
    }

    if (elapsed_ms >= duration_ms) {
        return OLED_WIDTH;
    }

    t = (float)elapsed_ms / (float)duration_ms;
    eased = t * t * (3.0f - 2.0f * t);
    shift = (int32_t)(eased * (float)OLED_WIDTH + 0.5f);
    if (shift < 0) {
        shift = 0;
    } else if (shift > OLED_WIDTH) {
        shift = OLED_WIDTH;
    }
    return (int16_t)shift;
}

/**
 * @brief Queue a page-originated model mutation for the app orchestrator.
 *
 * @param[in] mutation Mutation descriptor to enqueue. Must not be NULL.
 * @return true when the mutation was queued; false when the descriptor is invalid or the queue is full.
 * @throws None.
 */
static bool ui_enqueue_model_mutation(const UiModelMutation *mutation)
{
    if (mutation == NULL || mutation->type == UI_MODEL_MUTATION_NONE) {
        return false;
    }
    if (g_ui_model_mutation_count >= UI_MODEL_MUTATION_QUEUE_CAPACITY) {
        diag_service_note_ui_mutation_overflow(g_ui_model_mutation_count);
        return false;
    }

    g_ui_model_mutation_queue[g_ui_model_mutation_tail] = *mutation;
    g_ui_model_mutation_tail = (uint8_t)((g_ui_model_mutation_tail + 1U) % UI_MODEL_MUTATION_QUEUE_CAPACITY);
    g_ui_model_mutation_count++;
    return true;
}

static void ui_enqueue_bool_mutation(UiModelMutationType type, bool enabled)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = type;
    mutation.data.enabled = enabled;
    (void)ui_enqueue_model_mutation(&mutation);
}

static void ui_enqueue_u8_mutation(UiModelMutationType type, uint8_t value)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = type;
    mutation.data.u8 = value;
    (void)ui_enqueue_model_mutation(&mutation);
}

static void ui_enqueue_u32_mutation(UiModelMutationType type, uint32_t value)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = type;
    mutation.data.u32 = value;
    (void)ui_enqueue_model_mutation(&mutation);
}

static void ui_enqueue_i8_mutation(UiModelMutationType type, int8_t value)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = type;
    mutation.data.delta_i8 = value;
    (void)ui_enqueue_model_mutation(&mutation);
}

static void ui_enqueue_i32_mutation(UiModelMutationType type, int32_t value)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = type;
    mutation.data.delta_i32 = value;
    (void)ui_enqueue_model_mutation(&mutation);
}

static void ui_enqueue_game_score_mutation(GameId game_id, uint16_t score)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_SET_GAME_HIGH_SCORE;
    mutation.data.game_high_score.game_id = (uint8_t)game_id;
    mutation.data.game_high_score.score = score;
    (void)ui_enqueue_model_mutation(&mutation);
}


const char *ui_weekday_name(uint8_t weekday)
{
    return g_weekdays[weekday % 7U];
}

/**
 * @brief Build a consolidated read-only system snapshot for page presenters.
 *
 * @param[out] out Snapshot destination. Must not be NULL.
 * @return void
 * @throws None.
 */
void ui_get_system_snapshot(UiSystemSnapshot *out)
{
    if (out == NULL) {
        return;
    }

    ui_snapshot_build(out, platform_time_now_ms());
}

bool ui_get_runtime_snapshot(UiRuntimeSnapshot *out)
{
    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->current = g_ui.current;
    out->from = g_ui.from;
    out->to = g_ui.to;
    out->animating = g_ui.animating;
    out->transition_render = g_ui.transition_render;
    out->dir = g_ui.dir;
    out->anim_start_ms = g_ui.anim_start_ms;
    out->last_input_ms = g_ui.last_input_ms;
    out->last_render_ms = g_ui.last_render_ms;
    out->sleeping = g_ui.sleeping;
    out->dirty = g_ui.dirty;
    out->last_popup = g_ui.last_popup;
    return g_ui_initialized;
}

void ui_runtime_set_sleeping(bool sleeping)
{
    g_ui.sleeping = sleeping;
}

/**
 * @brief Forward a sensor reinitialization request outside page code.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_sensor_reinit(void)
{
    g_ui_pending_actions |= UI_ACTION_SENSOR_REINIT;
}

/**
 * @brief Forward a sensor calibration request outside page code.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_sensor_calibration(void)
{
    g_ui_pending_actions |= UI_ACTION_SENSOR_CALIBRATION;
}

/**
 * @brief Forward a storage flush request outside page code.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_storage_manual_flush(void)
{
    g_ui_pending_actions |= UI_ACTION_STORAGE_MANUAL_FLUSH;
}

/**
 * @brief Clear safe mode through the presenter boundary.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_clear_safe_mode(void)
{
    g_ui_pending_actions |= UI_ACTION_CLEAR_SAFE_MODE;
}

void ui_request_sleep(SleepReason reason)
{
#if defined(APP_DISABLE_SLEEP) && APP_DISABLE_SLEEP
    (void)reason;
    return;
#else
    if (reason == SLEEP_REASON_MANUAL) {
        g_ui_pending_actions |= UI_ACTION_SLEEP_MANUAL;
    } else if (reason == SLEEP_REASON_AUTO_TIMEOUT) {
        g_ui_pending_actions |= UI_ACTION_SLEEP_AUTO;
    }
#endif
}

uint32_t ui_consume_actions(void)
{
    uint32_t actions = g_ui_pending_actions;
    g_ui_pending_actions = UI_ACTION_NONE;
    return actions;
}

bool ui_consume_model_mutation(UiModelMutation *out)
{
    if (out == NULL || g_ui_model_mutation_count == 0U) {
        return false;
    }

    *out = g_ui_model_mutation_queue[g_ui_model_mutation_head];
    g_ui_model_mutation_head = (uint8_t)((g_ui_model_mutation_head + 1U) % UI_MODEL_MUTATION_QUEUE_CAPACITY);
    g_ui_model_mutation_count--;
    return true;
}

bool ui_has_pending_actions(void)
{
    return g_ui_pending_actions != UI_ACTION_NONE || g_ui_model_mutation_count != 0U;
}

void ui_request_set_brightness(uint8_t level)
{
    ui_enqueue_u8_mutation(UI_MODEL_MUTATION_SET_BRIGHTNESS, level);
}

void ui_request_set_goal(uint32_t goal)
{
    ui_enqueue_u32_mutation(UI_MODEL_MUTATION_SET_GOAL, goal);
}

void ui_request_cycle_watchface(int8_t dir)
{
    ui_enqueue_i8_mutation(UI_MODEL_MUTATION_CYCLE_WATCHFACE, dir);
}

void ui_request_cycle_screen_timeout(int8_t dir)
{
    ui_enqueue_i8_mutation(UI_MODEL_MUTATION_CYCLE_SCREEN_TIMEOUT, dir);
}

void ui_request_set_sensor_sensitivity(uint8_t level)
{
    ui_enqueue_u8_mutation(UI_MODEL_MUTATION_SET_SENSOR_SENSITIVITY, level);
}

void ui_request_set_auto_wake(bool enabled)
{
    ui_enqueue_bool_mutation(UI_MODEL_MUTATION_SET_AUTO_WAKE, enabled);
}

void ui_request_set_auto_sleep(bool enabled)
{
    ui_enqueue_bool_mutation(UI_MODEL_MUTATION_SET_AUTO_SLEEP, enabled);
}

void ui_request_set_dnd(bool enabled)
{
    ui_enqueue_bool_mutation(UI_MODEL_MUTATION_SET_DND, enabled);
}

void ui_request_set_show_seconds(bool enabled)
{
    ui_enqueue_bool_mutation(UI_MODEL_MUTATION_SET_SHOW_SECONDS, enabled);
}

void ui_request_set_animations(bool enabled)
{
    ui_enqueue_bool_mutation(UI_MODEL_MUTATION_SET_ANIMATIONS, enabled);
}

void ui_request_restore_defaults(void)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_RESTORE_DEFAULTS;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_select_alarm_offset(int8_t delta)
{
    ui_enqueue_i8_mutation(UI_MODEL_MUTATION_SELECT_ALARM_OFFSET, delta);
}

void ui_request_set_alarm_enabled_at(uint8_t index, bool enabled)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_SET_ALARM_ENABLED_AT;
    mutation.data.alarm_enabled.index = index;
    mutation.data.alarm_enabled.enabled = enabled;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_set_alarm_time_at(uint8_t index, uint8_t hour, uint8_t minute)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_SET_ALARM_TIME_AT;
    mutation.data.alarm_time.index = index;
    mutation.data.alarm_time.hour = hour;
    mutation.data.alarm_time.minute = minute;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_set_alarm_repeat_mask_at(uint8_t index, uint8_t repeat_mask)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_SET_ALARM_REPEAT_MASK_AT;
    mutation.data.alarm_repeat.index = index;
    mutation.data.alarm_repeat.repeat_mask = repeat_mask;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_stopwatch_toggle(uint32_t now_ms)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_STOPWATCH_TOGGLE;
    mutation.data.now_ms = now_ms;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_stopwatch_reset(void)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_STOPWATCH_RESET;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_stopwatch_lap(void)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_STOPWATCH_LAP;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_timer_toggle(uint32_t now_ms)
{
    UiModelMutation mutation;

    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_TIMER_TOGGLE;
    mutation.data.now_ms = now_ms;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_timer_adjust_seconds(int32_t delta_s)
{
    ui_enqueue_i32_mutation(UI_MODEL_MUTATION_TIMER_ADJUST_SECONDS, delta_s);
}

void ui_request_timer_cycle_preset(int8_t dir)
{
    ui_enqueue_i8_mutation(UI_MODEL_MUTATION_TIMER_CYCLE_PRESET, dir);
}

void ui_request_set_datetime(const DateTime *dt)
{
    UiModelMutation mutation;

    if (dt == NULL) {
        return;
    }
    memset(&mutation, 0, sizeof(mutation));
    mutation.type = UI_MODEL_MUTATION_SET_DATETIME;
    mutation.data.date_time = *dt;
    (void)ui_enqueue_model_mutation(&mutation);
}

void ui_request_set_game_high_score(GameId game_id, uint16_t score)
{
    if ((uint8_t)game_id >= (uint8_t)GAME_ID_COUNT) {
        return;
    }
    ui_enqueue_game_score_mutation(game_id, score);
}

/**
 * @brief Apply service-side sensor sensitivity from the current model settings.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_sync_sensor_setting_from_model(void)
{
    g_ui_pending_actions |= UI_ACTION_SYNC_SENSOR_SETTINGS;
}

/**
 * @brief Restore defaults and synchronize dependent runtime services through the dispatcher boundary.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_restore_defaults_and_sync_services(void)
{
    ui_request_restore_defaults();
}

void ui_core_mark_dirty(void)
{
    g_ui.dirty = true;
}

void ui_request_render(void)
{
    ui_core_mark_dirty();
}

void ui_core_haptic_soft(void)
{
#if APP_FEATURE_VIBRATION
    ModelDomainState domain_state;
    if (model_get_domain_state(&domain_state) != NULL && domain_state.settings.vibrate && !domain_state.settings.dnd) {
        vibe_pulse(18);
    }
#endif
}

void ui_core_haptic_confirm(void)
{
#if APP_FEATURE_VIBRATION
    ModelDomainState domain_state;
    if (model_get_domain_state(&domain_state) != NULL && domain_state.settings.vibrate && !domain_state.settings.dnd) {
        vibe_pulse(30);
    }
#endif
}

void ui_core_apply_brightness(void)
{
    ModelDomainState domain_state;

    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }
    display_service_apply_settings(&domain_state.settings);
}

void ui_core_draw_header(int16_t x, const char *title)
{
    int16_t pill_w = display_text_width_5x7(title) + 10;
    int16_t line_x = x + pill_w + 8;

    if (pill_w < 26) {
        pill_w = 26;
    }

    display_fill_round_rect(x + 4, 1, pill_w, 9, true);
    display_draw_text_5x7(x + 9, 2, title, false);
    if (line_x < x + 123) {
        display_draw_hline(line_x, 5, (x + 123) - line_x, true);
    }
    display_draw_hline(x + 4, UI_LAYOUT_HEADER_LINE_Y, 120, true);
}

void ui_core_draw_footer_hint(int16_t x, const char *text)
{
    if (text == NULL || *text == '\0') {
        return;
    }

    display_draw_hline(x + 4, UI_LAYOUT_FOOTER_LINE_Y, 120, true);
    display_draw_text_centered_5x7(x, UI_LAYOUT_FOOTER_TEXT_Y, 128, text, true);
}

void ui_core_draw_scrollbar(int16_t x, int16_t y, int16_t h, uint8_t total, uint8_t index)
{
    int16_t thumb_h;
    int16_t travel;
    int16_t thumb_y;

    if (total <= 1U) return;

    display_draw_vline(x + 1, y, h, true);
    {
        thumb_h = h / total;
        if (thumb_h < 7) thumb_h = 7;
        travel = h - thumb_h;
        thumb_y = y + ((total > 1U) ? ((travel * index) / (total - 1U)) : 0);
        display_fill_round_rect(x, thumb_y, 3, thumb_h, true);
    }
}

void ui_core_draw_status_bar(int16_t ox)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    int16_t wifi_x;

    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL) {
        return;
    }
    if (runtime_state.battery_present) {
        display_draw_battery_icon(ox + 108, 1, runtime_state.battery_percent, runtime_state.charging, false);
        wifi_x = ox + 90;
    } else {
        display_draw_text_5x7(ox + 102, 2, "USB", true);
        wifi_x = ox + 84;
    }
    display_draw_wifi_icon(wifi_x, 0, web_wifi_connected(), false);
    if (model_find_next_enabled_alarm() < APP_MAX_ALARMS) display_draw_alarm_icon(ox + 1, 1, false);
    if (domain_state.settings.dnd) display_draw_text_5x7(ox + 14, 2, "DND", true);
}

void ui_status_compose_header_tags(char *out, size_t out_size)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    NetworkSyncSnapshot net_snapshot;
    uint8_t alarm_index;
    bool has_wifi = false;
    bool has_sync = false;
    bool has_low = false;
    bool has_alarm = false;

    if (out == NULL || out_size == 0U) {
        return;
    }

    out[0] = '\0';
    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL) {
        return;
    }

    memset(&net_snapshot, 0, sizeof(net_snapshot));
    (void)network_sync_service_get_snapshot(&net_snapshot);
    alarm_index = model_find_next_enabled_alarm();
    has_wifi = net_snapshot.wifi_connected || web_wifi_connected();
    has_sync = domain_state.time_valid && (domain_state.time_state == TIME_STATE_RECOVERED || net_snapshot.time_synced);
    has_low = runtime_state.battery_present && runtime_state.battery_percent <= app_tuning_manifest_get()->low_battery_threshold;
    has_alarm = domain_state.alarm_ringing_index < APP_MAX_ALARMS || alarm_index < APP_MAX_ALARMS;

    if (has_sync) strncat(out, "SYNC ", out_size - strlen(out) - 1U);
    if (has_wifi) strncat(out, "WIFI ", out_size - strlen(out) - 1U);
    if (domain_state.settings.dnd) strncat(out, "DND ", out_size - strlen(out) - 1U);
    if (has_alarm) strncat(out, "ALM ", out_size - strlen(out) - 1U);
    if (has_low) strncat(out, "LOW ", out_size - strlen(out) - 1U);
    {
        size_t len = strlen(out);
        while (len > 0U && out[len - 1U] == ' ') {
            out[--len] = '\0';
        }
    }
}

void ui_status_compose_alarm_value(char *out, size_t out_size)
{
    uint8_t index;

    if (out == NULL || out_size == 0U) return;
    index = model_find_next_enabled_alarm();
    if (index < APP_MAX_ALARMS) {
        const AlarmState *alarm = model_get_alarm(index);
        snprintf(out, out_size, alarm->ringing ? "RING" : "%02u:%02u", alarm->hour, alarm->minute);
    } else {
        snprintf(out, out_size, "OFF");
    }
}

void ui_status_compose_activity_value(char *out, size_t out_size)
{
    ModelDomainState domain_state;
    uint8_t pct;

    if (out == NULL || out_size == 0U) return;
    if (model_get_domain_state(&domain_state) == NULL) {
        out[0] = '\0';
        return;
    }
    pct = (uint8_t)APP_CLAMP((domain_state.activity.goal == 0U) ? 0U :
                             (domain_state.activity.steps * 100UL) / domain_state.activity.goal, 0U, 100U);
    snprintf(out, out_size, "%u%%", pct);
}

void ui_status_compose_sensor_value(char *out, size_t out_size)
{
    ModelRuntimeState runtime_state;

    if (out == NULL || out_size == 0U) return;
    if (model_get_runtime_state(&runtime_state) == NULL) {
        out[0] = '\0';
        return;
    }
    if (!runtime_state.sensor.online) {
        snprintf(out, out_size, "OFF");
    } else {
        snprintf(out, out_size, "ON/Q%u", runtime_state.sensor.quality);
    }
}

void ui_status_compose_storage_value(char *out, size_t out_size)
{
    ModelRuntimeState runtime_state;

    if (out == NULL || out_size == 0U) return;
    if (model_get_runtime_state(&runtime_state) == NULL) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_size, runtime_state.storage_crc_ok ? "OK" : "BAD");
}

void ui_status_compose_diag_value(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0U) return;
    snprintf(out, out_size, diag_service_is_safe_mode_active() ? "SAFE" : "");
}

void ui_status_compose_network_value(char *line, size_t line_size, char *subline, size_t subline_size)
{
    ModelDomainState domain_state;
    NetworkSyncSnapshot net_snapshot;
    TimeSourceSnapshot time_snapshot;
    uint32_t now_ms = platform_time_now_ms();
    uint32_t age_s = 0U;

    if (line != NULL && line_size > 0U) line[0] = '\0';
    if (subline != NULL && subline_size > 0U) subline[0] = '\0';
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }

    memset(&net_snapshot, 0, sizeof(net_snapshot));
    memset(&time_snapshot, 0, sizeof(time_snapshot));
    (void)network_sync_service_get_snapshot(&net_snapshot);
    (void)time_service_get_source_snapshot(&time_snapshot);
    if (net_snapshot.last_weather_sync_ms != 0U && now_ms >= net_snapshot.last_weather_sync_ms) {
        age_s = (now_ms - net_snapshot.last_weather_sync_ms) / 1000UL;
    }

    if (!net_snapshot.wifi_connected && !web_wifi_connected()) {
        snprintf(line, line_size, "WIFI OFFLINE");
        snprintf(subline, subline_size, domain_state.time_valid ? "TIME %s" : "TIME UNSYNC",
                 domain_state.time_state == TIME_STATE_RECOVERED ? "RECOVERED" : "LOCAL");
    } else if (net_snapshot.weather_valid) {
        snprintf(line, line_size, "%d.%dC %s",
                 (int)(net_snapshot.temperature_tenths_c / 10),
                 (int)((net_snapshot.temperature_tenths_c < 0 ? -net_snapshot.temperature_tenths_c : net_snapshot.temperature_tenths_c) % 10),
                 net_snapshot.weather_text);
        snprintf(subline, subline_size, "%s  %lus",
                 time_service_source_name(time_snapshot.source),
                 (unsigned long)age_s);
    } else if (domain_state.time_valid) {
        snprintf(line, line_size, net_snapshot.time_synced ? "TIME READY" : "TIME LOCAL");
        snprintf(subline, subline_size, "SRC %s  WEATHER SYNC", time_service_source_name(time_snapshot.source));
    } else {
        snprintf(line, line_size, "SYNC NEEDED");
        snprintf(subline, subline_size, "WIFI READY  OK REFRESH");
    }
}

void ui_core_draw_card(int16_t x, int16_t y, int16_t w, int16_t h, const char *title)
{
    display_draw_round_rect(x, y, w, h, true);
    if (title != NULL && *title != '\0') {
        int16_t tag_w = display_text_width_5x7(title) + 8;

        if (tag_w > (w - 8)) {
            tag_w = w - 8;
        }
        display_fill_round_rect(x + 4, y + 1, tag_w, 9, true);
        display_draw_text_5x7(x + 8, y + 2, title, false);
    }
}

void ui_core_draw_kv_row(int16_t x, int16_t y, int16_t w, const char *label, const char *value)
{
    if (label != NULL && *label != '\0') {
        display_draw_text_5x7(x + 6, y, label, true);
    }
    if (value != NULL && *value != '\0') {
        display_draw_text_right_5x7(x + w - 6, y, value, true);
    }
    display_draw_hline(x + 4, y + 8, w - 8, true);
}

void ui_core_draw_list_item(int16_t x, int16_t y, int16_t w, const char *label, const char *value, bool selected, bool accent)
{
    int16_t box_x = x + 6;
    bool text_color = true;

    if (selected) {
        display_fill_round_rect(box_x, y, w, 10, true);
        text_color = false;
    } else {
        display_draw_hline(box_x + 4, y + 10, w - 8, true);
    }

    if (accent) {
        display_fill_rect(box_x + 3, y + 2, 2, 6, selected ? false : true);
    }

    if (label != NULL && *label != '\0') {
        display_draw_text_5x7(box_x + 8, y + 2, label, text_color);
    }
    if (value != NULL && *value != '\0') {
        display_draw_text_right_5x7(box_x + w - 8, y + 2, value, text_color);
    }
}

void ui_init(void)
{
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui_model_mutation_head = 0U;
    g_ui_model_mutation_tail = 0U;
    g_ui_model_mutation_count = 0U;
    memset(g_ui_model_mutation_queue, 0, sizeof(g_ui_model_mutation_queue));
    g_ui.current = diag_service_is_safe_mode_active() ? PAGE_DIAG : PAGE_WATCHFACE;
    g_ui.last_input_ms = platform_time_now_ms();
    g_ui.last_popup = POPUP_NONE;
    g_ui.dirty = true;
    g_ui_pending_actions = UI_ACTION_NONE;
    ui_core_apply_brightness();
    power_service_set_sleeping(false);
    g_ui_initialized = true;
}

void ui_render(void)
{
    bool transition_render = g_ui.animating;

    if (g_ui.sleeping) return;
    display_service_begin_frame();
    ui_runtime_set_transition_render(transition_render);

    if (g_ui.animating) {
        uint32_t elapsed = platform_time_now_ms() - g_ui.anim_start_ms;
        if (elapsed > app_tuning_manifest_get()->ui_anim_duration_ms) elapsed = app_tuning_manifest_get()->ui_anim_duration_ms;
        {
            int16_t shift = ui_anim_eased_shift(elapsed, app_tuning_manifest_get()->ui_anim_duration_ms);
            int16_t from_x = (g_ui.dir > 0) ? -shift : shift;
            int16_t to_x = (g_ui.dir > 0) ? (OLED_WIDTH - shift) : (shift - OLED_WIDTH);
            ui_core_render_page(g_ui.from, from_x);
            ui_core_render_page(g_ui.to, to_x);
        }
    } else {
        ui_core_render_page(g_ui.current, 0);
    }

    ui_runtime_set_transition_render(false);
    ui_popup_render();
    display_service_end_frame();
    g_ui.last_render_ms = platform_time_now_ms();
    {
        ModelUiState ui_state;
        g_ui.last_popup = model_get_ui_state(&ui_state) != NULL ? ui_state.popup : POPUP_NONE;
    }
    g_ui.dirty = false;
    model_note_render(g_ui.last_render_ms);
}


bool ui_is_initialized(void)
{
    return g_ui_initialized;
}

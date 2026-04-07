#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "key.h"
#include "model.h"
#include "sensor.h"

typedef enum {
    PAGE_WATCHFACE = 0,
    PAGE_QUICK,
    PAGE_APPS,
    PAGE_ALARM,
    PAGE_ALARM_EDIT,
    PAGE_STOPWATCH,
    PAGE_TIMER,
    PAGE_ACTIVITY,
    PAGE_SENSOR,
    PAGE_SETTINGS,
    PAGE_TIMESET,
    PAGE_DIAG,
    PAGE_ABOUT,
    PAGE_CALIBRATION,
    PAGE_INPUTTEST,
    PAGE_STORAGE,
    PAGE_LIQUID,
    PAGE_COUNT
} PageId;

typedef enum {
    UI_FOCUS_NAV = 0,
    UI_FOCUS_EDIT,
    UI_FOCUS_POPUP,
    UI_FOCUS_SLEEP
} UiFocusMode;

typedef struct {
    PageId current;
    PageId from;
    PageId to;
    bool animating;
    bool transition_render;
    int8_t dir;
    uint32_t anim_start_ms;
    uint32_t last_input_ms;
    uint32_t last_render_ms;
    bool sleeping;
    bool dirty;
    uint8_t quick_index;
    uint8_t app_index;
    uint8_t settings_index;
    uint8_t alarm_field;
    uint8_t time_field;
    uint8_t timer_preset_index;
    bool settings_editing;
    bool alarm_editing;
    DateTime edit_time;
    PopupType last_popup;
} UiState;

typedef enum {
    UI_REFRESH_NONE = 0,
    UI_REFRESH_WATCHFACE,
    UI_REFRESH_CARD,
    UI_REFRESH_SENSOR,
    UI_REFRESH_LIQUID,
    UI_REFRESH_TIMER,
    UI_REFRESH_STOPWATCH
} UiPageRefreshPolicy;

typedef enum {
    UI_NAV_GROUP_ROOT = 0,
    UI_NAV_GROUP_APPS,
    UI_NAV_GROUP_SETTINGS,
    UI_NAV_GROUP_SYSTEM,
    UI_NAV_GROUP_EDIT
} UiNavGroup;

typedef enum {
    UI_PRESENTER_NONE = 0,
    UI_PRESENTER_WATCHFACE,
    UI_PRESENTER_ACTIVITY,
    UI_PRESENTER_SENSOR,
    UI_PRESENTER_SETTINGS,
    UI_PRESENTER_DIAG,
    UI_PRESENTER_STORAGE,
    UI_PRESENTER_SYSTEM
} UiPagePresenterId;

typedef struct {
    uint32_t steps;
    uint32_t goal;
    uint8_t goal_percent;
    uint8_t activity_level;
    MotionState motion_state;
    uint16_t active_minutes;
} UiActivityView;

typedef struct {
    bool online;
    bool calibrated;
    bool static_now;
    SensorRuntimeState runtime_state;
    SensorCalibrationState calibration_state;
    const char *runtime_state_name;
    const char *calibration_state_name;
    uint8_t quality;
    uint8_t error_code;
    uint8_t calibration_progress;
    uint8_t fault_count;
    uint8_t reinit_count;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int8_t pitch_deg;
    int8_t roll_deg;
    int16_t motion_score;
    uint32_t retry_backoff_s;
    uint32_t offline_elapsed_s;
} UiSensorView;

typedef struct {
    uint8_t brightness;
    bool auto_wake;
    bool auto_sleep;
    bool dnd;
    uint32_t goal;
    uint8_t watchface;
    bool show_seconds;
    bool animations;
    uint32_t screen_timeout_s;
    uint8_t sensor_sensitivity;
    bool time_valid;
    DateTime current_time;
} UiSettingsView;

typedef struct {
    uint8_t version;
    bool initialized;
    bool crc_valid;
    uint8_t pending_mask;
    uint8_t dirty_source_mask;
    uint16_t stored_crc;
    uint16_t calculated_crc;
    uint32_t commit_count;
    bool last_commit_ok;
    const char *last_commit_reason_name;
    const char *backend_name;
    const char *commit_state_name;
    bool transaction_active;
    bool sleep_flush_pending;
    uint32_t wear_count;
} UiStorageView;

typedef enum {
    UI_ACTION_NONE = 0,
    UI_ACTION_SENSOR_REINIT = 1 << 0,
    UI_ACTION_SENSOR_CALIBRATION = 1 << 1,
    UI_ACTION_STORAGE_MANUAL_FLUSH = 1 << 2,
    UI_ACTION_CLEAR_SAFE_MODE = 1 << 3,
    UI_ACTION_SYNC_SENSOR_SETTINGS = 1 << 4,
    UI_ACTION_SLEEP_MANUAL = 1 << 5,
    UI_ACTION_SLEEP_AUTO = 1 << 6
} UiActionFlags;

#define UI_MODEL_MUTATION_QUEUE_CAPACITY 16U

typedef enum {
    UI_MODEL_MUTATION_NONE = 0,
    UI_MODEL_MUTATION_SET_BRIGHTNESS,
    UI_MODEL_MUTATION_SET_GOAL,
    UI_MODEL_MUTATION_CYCLE_WATCHFACE,
    UI_MODEL_MUTATION_CYCLE_SCREEN_TIMEOUT,
    UI_MODEL_MUTATION_SET_SENSOR_SENSITIVITY,
    UI_MODEL_MUTATION_SET_AUTO_WAKE,
    UI_MODEL_MUTATION_SET_AUTO_SLEEP,
    UI_MODEL_MUTATION_SET_DND,
    UI_MODEL_MUTATION_SET_SHOW_SECONDS,
    UI_MODEL_MUTATION_SET_ANIMATIONS,
    UI_MODEL_MUTATION_RESTORE_DEFAULTS,
    UI_MODEL_MUTATION_SELECT_ALARM_OFFSET,
    UI_MODEL_MUTATION_SET_ALARM_ENABLED_AT,
    UI_MODEL_MUTATION_SET_ALARM_TIME_AT,
    UI_MODEL_MUTATION_SET_ALARM_REPEAT_MASK_AT,
    UI_MODEL_MUTATION_STOPWATCH_TOGGLE,
    UI_MODEL_MUTATION_STOPWATCH_RESET,
    UI_MODEL_MUTATION_STOPWATCH_LAP,
    UI_MODEL_MUTATION_TIMER_TOGGLE,
    UI_MODEL_MUTATION_TIMER_ADJUST_SECONDS,
    UI_MODEL_MUTATION_TIMER_CYCLE_PRESET,
    UI_MODEL_MUTATION_SET_DATETIME
} UiModelMutationType;

typedef struct {
    UiModelMutationType type;
    union {
        uint8_t u8;
        uint32_t u32;
        int8_t delta_i8;
        int32_t delta_i32;
        bool enabled;
        uint32_t now_ms;
        DateTime date_time;
        struct {
            uint8_t index;
            bool enabled;
        } alarm_enabled;
        struct {
            uint8_t index;
            uint8_t hour;
            uint8_t minute;
        } alarm_time;
        struct {
            uint8_t index;
            uint8_t repeat_mask;
        } alarm_repeat;
    } data;
} UiModelMutation;

typedef struct {
    ModelDomainState domain;
    ModelRuntimeState runtime;
    ModelUiState ui;
    UiActivityView activity;
    UiSensorView sensor;
    UiSettingsView settings;
    UiStorageView storage;
    const char *storage_backend_name;
    const char *storage_commit_state_name;
    bool storage_transaction_active;
    bool storage_sleep_flush_pending;
    uint32_t storage_wear_count;
    bool safe_mode_active;
    bool safe_mode_can_clear;
    const char *safe_mode_reason_name;
    bool has_last_fault;
    const char *last_fault_name;
    const char *last_fault_severity_name;
    uint8_t last_fault_count;
    uint16_t last_fault_value;
    uint8_t last_fault_aux;
    uint8_t retained_checkpoint;
    uint32_t retained_max_loop_ms;
    const char *reset_reason_name;
    const char *wake_reason_name;
    const char *sleep_reason_name;
    const char *time_state_name;
    const char *storage_last_commit_reason_name;
    const char *wdt_last_checkpoint_name;
    const char *wdt_last_checkpoint_result_name;
    uint32_t wdt_max_loop_ms;
    bool has_last_log;
    const char *last_log_name;
    uint16_t last_log_value;
    uint8_t last_log_aux;
    uint32_t display_tx_fail_count;
    uint32_t ui_mutation_overflow_event_count;
    bool key_up_down;
    bool key_down_down;
    bool key_ok_down;
    bool key_back_down;
} UiSystemSnapshot;

typedef struct {
    PageId current;
    PageId from;
    PageId to;
    bool animating;
    bool transition_render;
    int8_t dir;
    uint32_t anim_start_ms;
    uint32_t last_input_ms;
    uint32_t last_render_ms;
    bool sleeping;
    bool dirty;
    PopupType last_popup;
} UiRuntimeSnapshot;

typedef struct {
    const char *title;
    UiPageRefreshPolicy refresh_policy;
    bool allow_auto_sleep;
    bool allow_popup;
    uint8_t nav_group;
    uint8_t presenter;
    void (*render)(PageId page, int16_t ox);
    bool (*handle)(PageId page, const KeyEvent *e, uint32_t now_ms);
} UiPageOps;

extern UiState g_ui;

static inline PageId ui_runtime_get_current_page(void) { return g_ui.current; }
static inline void ui_runtime_set_current_page(PageId page) { g_ui.current = page; }
static inline PageId ui_runtime_get_from_page(void) { return g_ui.from; }
static inline void ui_runtime_set_from_page(PageId page) { g_ui.from = page; }
static inline PageId ui_runtime_get_to_page(void) { return g_ui.to; }
static inline void ui_runtime_set_to_page(PageId page) { g_ui.to = page; }
static inline bool ui_runtime_is_animating(void) { return g_ui.animating; }
static inline void ui_runtime_set_animating(bool animating) { g_ui.animating = animating; }
static inline bool ui_runtime_is_transition_render(void) { return g_ui.transition_render; }
static inline void ui_runtime_set_transition_render(bool transition_render) { g_ui.transition_render = transition_render; }
static inline int8_t ui_runtime_get_nav_dir(void) { return g_ui.dir; }
static inline void ui_runtime_set_nav_dir(int8_t dir) { g_ui.dir = dir; }
static inline uint32_t ui_runtime_get_anim_start_ms(void) { return g_ui.anim_start_ms; }
static inline void ui_runtime_set_anim_start_ms(uint32_t now_ms) { g_ui.anim_start_ms = now_ms; }
static inline uint32_t ui_runtime_get_last_input_ms(void) { return g_ui.last_input_ms; }
static inline void ui_runtime_set_last_input_ms(uint32_t now_ms) { g_ui.last_input_ms = now_ms; }
static inline uint32_t ui_runtime_get_last_render_ms(void) { return g_ui.last_render_ms; }
static inline void ui_runtime_set_last_render_ms(uint32_t now_ms) { g_ui.last_render_ms = now_ms; }
static inline bool ui_runtime_is_sleeping_inline(void) { return g_ui.sleeping; }
static inline bool ui_runtime_is_dirty(void) { return g_ui.dirty; }
static inline void ui_runtime_set_dirty(bool dirty) { g_ui.dirty = dirty; }
static inline PopupType ui_runtime_get_last_popup(void) { return g_ui.last_popup; }
static inline void ui_runtime_set_last_popup(PopupType popup) { g_ui.last_popup = popup; }
static inline uint8_t ui_runtime_get_quick_index(void) { return g_ui.quick_index; }
static inline void ui_runtime_set_quick_index(uint8_t index) { g_ui.quick_index = index; }
static inline uint8_t ui_runtime_get_app_index(void) { return g_ui.app_index; }
static inline void ui_runtime_set_app_index(uint8_t index) { g_ui.app_index = index; }
static inline uint8_t ui_runtime_get_settings_index(void) { return g_ui.settings_index; }
static inline void ui_runtime_set_settings_index(uint8_t index) { g_ui.settings_index = index; }
static inline uint8_t ui_runtime_get_alarm_field(void) { return g_ui.alarm_field; }
static inline void ui_runtime_set_alarm_field(uint8_t field) { g_ui.alarm_field = field; }
static inline uint8_t ui_runtime_get_time_field(void) { return g_ui.time_field; }
static inline void ui_runtime_set_time_field(uint8_t field) { g_ui.time_field = field; }
static inline bool ui_runtime_is_settings_editing(void) { return g_ui.settings_editing; }
static inline void ui_runtime_set_settings_editing(bool editing) { g_ui.settings_editing = editing; }
static inline bool ui_runtime_is_alarm_editing(void) { return g_ui.alarm_editing; }
static inline void ui_runtime_set_alarm_editing(bool editing) { g_ui.alarm_editing = editing; }
static inline DateTime ui_runtime_get_edit_time(void) { return g_ui.edit_time; }
static inline void ui_runtime_set_edit_time(const DateTime *dt) { if (dt != NULL) { g_ui.edit_time = *dt; } }

const char *ui_weekday_name(uint8_t weekday);
void ui_core_mark_dirty(void);
void ui_core_go(PageId next, int8_t dir, uint32_t now_ms);
void ui_core_draw_header(int16_t x, const char *title);
void ui_core_draw_footer_hint(int16_t x, const char *text);
void ui_core_draw_scrollbar(int16_t x, int16_t y, int16_t h, uint8_t total, uint8_t index);
void ui_core_draw_status_bar(int16_t ox);
void ui_core_draw_card(int16_t x, int16_t y, int16_t w, int16_t h, const char *title);
void ui_core_draw_kv_row(int16_t x, int16_t y, int16_t w, const char *label, const char *value);
void ui_core_draw_list_item(int16_t x, int16_t y, int16_t w, const char *label, const char *value, bool selected, bool accent);
void ui_core_apply_brightness(void);
void ui_core_haptic_soft(void);
void ui_core_haptic_confirm(void);
void ui_core_wake_internal(WakeReason reason);

void ui_page_watchface_render(PageId page, int16_t ox);
bool ui_page_watchface_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_quick_render(PageId page, int16_t ox);
bool ui_page_quick_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_apps_render(PageId page, int16_t ox);
bool ui_page_apps_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_home_render(PageId page, int16_t ox);
bool ui_page_home_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_alarm_render(PageId page, int16_t ox);
bool ui_page_alarm_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_alarm_edit_render(PageId page, int16_t ox);
bool ui_page_alarm_edit_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_stopwatch_render(PageId page, int16_t ox);
bool ui_page_stopwatch_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_timer_render(PageId page, int16_t ox);
bool ui_page_timer_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_tools_render(PageId page, int16_t ox);
bool ui_page_tools_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_settings_main_render(PageId page, int16_t ox);
bool ui_page_settings_main_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_time_set_render(PageId page, int16_t ox);
bool ui_page_time_set_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_settings_render(PageId page, int16_t ox);
bool ui_page_settings_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_system_render(PageId page, int16_t ox);
bool ui_page_system_handle(PageId page, const KeyEvent *e, uint32_t now_ms);

void ui_page_activity_render(PageId page, int16_t ox);
bool ui_page_activity_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_sensor_render(PageId page, int16_t ox);
bool ui_page_sensor_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_diag_render(PageId page, int16_t ox);
bool ui_page_diag_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_about_render(PageId page, int16_t ox);
bool ui_page_about_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_calibration_render(PageId page, int16_t ox);
bool ui_page_calibration_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_inputtest_render(PageId page, int16_t ox);
bool ui_page_inputtest_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_storage_render(PageId page, int16_t ox);
bool ui_page_storage_handle(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_page_liquid_render(PageId page, int16_t ox);
bool ui_page_liquid_handle(PageId page, const KeyEvent *e, uint32_t now_ms);

void ui_core_render_page(PageId page, int16_t ox);
bool ui_core_handle_page_event(PageId page, const KeyEvent *e, uint32_t now_ms);
void ui_popup_render(void);
bool ui_popup_handle_event(const KeyEvent *e);

UiFocusMode ui_focus_get_mode(void);
bool ui_focus_is_editing(void);
bool ui_nav_policy_handle_global(const KeyEvent *e, uint32_t now_ms);

const UiPageOps *ui_page_registry_get(PageId page);
void ui_get_system_snapshot(UiSystemSnapshot *out);
bool ui_get_runtime_snapshot(UiRuntimeSnapshot *out);
void ui_runtime_set_sleeping(bool sleeping);
/**
 * @brief Queue a sensor reinitialization action for the app orchestrator.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_sensor_reinit(void);

/**
 * @brief Queue a sensor calibration action for the app orchestrator.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_sensor_calibration(void);

/**
 * @brief Queue a manual storage flush action for the app orchestrator.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_storage_manual_flush(void);

/**
 * @brief Queue a safe-mode clear request for the app orchestrator.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_request_clear_safe_mode(void);

/**
 * @brief Queue a sleep request for the app orchestrator.
 *
 * @param[in] reason Requested sleep reason. Unsupported values are ignored.
 * @return void
 * @throws None.
 */
void ui_request_sleep(SleepReason reason);

/**
 * @brief Consume and clear all pending UI actions.
 *
 * @param None.
 * @return Bitwise OR of UiActionFlags values.
 * @throws None.
 */
uint32_t ui_consume_actions(void);
bool ui_has_pending_actions(void);

/**
 * @brief Dequeue the next model mutation requested by UI pages.
 *
 * @param[out] out Destination for the dequeued mutation. Must not be NULL.
 * @return true when a mutation was dequeued; false when the queue is empty or the input is invalid.
 * @throws None.
 */
bool ui_consume_model_mutation(UiModelMutation *out);

/**
 * @brief Queue a settings brightness mutation for the app orchestrator.
 *
 * @param[in] level Target brightness level after page-side clamping.
 * @return void
 * @throws None.
 */
void ui_request_set_brightness(uint8_t level);
void ui_request_set_goal(uint32_t goal);
void ui_request_cycle_watchface(int8_t dir);
void ui_request_cycle_screen_timeout(int8_t dir);
void ui_request_set_sensor_sensitivity(uint8_t level);
void ui_request_set_auto_wake(bool enabled);
void ui_request_set_auto_sleep(bool enabled);
void ui_request_set_dnd(bool enabled);
void ui_request_set_show_seconds(bool enabled);
void ui_request_set_animations(bool enabled);
void ui_request_restore_defaults(void);
void ui_request_select_alarm_offset(int8_t delta);
void ui_request_set_alarm_enabled_at(uint8_t index, bool enabled);
void ui_request_set_alarm_time_at(uint8_t index, uint8_t hour, uint8_t minute);
void ui_request_set_alarm_repeat_mask_at(uint8_t index, uint8_t repeat_mask);
void ui_request_stopwatch_toggle(uint32_t now_ms);
void ui_request_stopwatch_reset(void);
void ui_request_stopwatch_lap(void);
void ui_request_timer_toggle(uint32_t now_ms);
void ui_request_timer_adjust_seconds(int32_t delta_s);
void ui_request_timer_cycle_preset(int8_t dir);
void ui_request_set_datetime(const DateTime *dt);

/**
 * @brief Queue sensor setting synchronization through the app orchestrator.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_sync_sensor_setting_from_model(void);

/**
 * @brief Restore defaults and rely on the app orchestrator for side effects.
 *
 * @param None.
 * @return void
 * @throws None.
 */
void ui_restore_defaults_and_sync_services(void);

#endif

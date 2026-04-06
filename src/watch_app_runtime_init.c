#include "watch_app_internal.h"
#include "board_features.h"
#include "crash_capsule.h"
#include "services/activity_service.h"
#include "services/alarm_service.h"
#include "services/alert_service.h"
#include "services/battery_service.h"
#include "services/diag_service.h"
#include "services/storage_service.h"
#include "services/display_service.h"
#include "services/input_service.h"
#include "services/power_service.h"
#include "services/sensor_service.h"
#include "services/wdt_service.h"
#include "system_init.h"
#include "ui.h"
#include "platform_api.h"
#include <string.h>

typedef WatchAppInitStageStatus (*WatchAppInitFn)(void);

typedef struct {
    WatchAppInitStage stage;
    WatchAppInitFn init_fn;
} WatchAppInitDescriptor;

static const char * const g_watch_app_init_stage_names[WATCH_APP_INIT_STAGE_COUNT] = {
    "NONE",
    "DIAG",
    "POWER",
    "DISPLAY",
    "INPUT",
    "MODEL",
    "SENSOR",
    "ACTIVITY",
    "ALERT",
    "ALARM",
    "WDT",
    "BATTERY",
    "UI",
};

static const char * const g_watch_app_init_status_names[] = {
    "UNATTEMPTED",
    "OK",
    "DEGRADED",
    "SKIPPED",
    "FAILED",
};

static WatchAppInitStageStatus watch_app_init_stage_diag(void)
{
    diag_service_init();
    return WATCH_APP_INIT_STATUS_OK;
}

static WatchAppInitStageStatus watch_app_init_stage_power(void)
{
    power_service_init();
    return (!power_service_is_sleeping() && power_service_get_last_wake_reason() == WAKE_REASON_BOOT) ?
           WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_display(void)
{
    display_service_init();
    return display_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_input(void)
{
    input_service_init();
    return input_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_model(void)
{
    ModelDomainState domain_state;

    model_init();
    return model_get_domain_state(&domain_state) != NULL ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_sensor(void)
{
#if APP_FEATURE_SENSOR
    sensor_service_init();
    return sensor_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
#else
    return WATCH_APP_INIT_STATUS_SKIPPED;
#endif
}

static WatchAppInitStageStatus watch_app_init_stage_activity(void)
{
#if APP_FEATURE_SENSOR
    activity_service_init();
    return activity_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
#else
    return WATCH_APP_INIT_STATUS_SKIPPED;
#endif
}

static WatchAppInitStageStatus watch_app_init_stage_alert(void)
{
    alert_service_init();
    return alert_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_alarm(void)
{
    alarm_service_init();
    return alarm_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_wdt(void)
{
    wdt_service_init();
#if APP_FEATURE_WATCHDOG
    if (!system_runtime_watchdog_available()) {
        return WATCH_APP_INIT_STATUS_DEGRADED;
    }
#endif
    return wdt_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_battery(void)
{
    battery_service_init();
#if APP_FEATURE_BATTERY
    if (!system_runtime_battery_adc_available()) {
        return WATCH_APP_INIT_STATUS_DEGRADED;
    }
#else
    return WATCH_APP_INIT_STATUS_SKIPPED;
#endif
    return battery_service_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static WatchAppInitStageStatus watch_app_init_stage_ui(void)
{
    ui_init();
    return ui_is_initialized() ? WATCH_APP_INIT_STATUS_OK : WATCH_APP_INIT_STATUS_FAILED;
}

static const WatchAppInitDescriptor g_watch_app_init_descriptors[] = {
    {WATCH_APP_INIT_STAGE_DIAG, watch_app_init_stage_diag},
    {WATCH_APP_INIT_STAGE_POWER, watch_app_init_stage_power},
    {WATCH_APP_INIT_STAGE_DISPLAY, watch_app_init_stage_display},
    {WATCH_APP_INIT_STAGE_INPUT, watch_app_init_stage_input},
    {WATCH_APP_INIT_STAGE_MODEL, watch_app_init_stage_model},
    {WATCH_APP_INIT_STAGE_SENSOR, watch_app_init_stage_sensor},
    {WATCH_APP_INIT_STAGE_ACTIVITY, watch_app_init_stage_activity},
    {WATCH_APP_INIT_STAGE_ALERT, watch_app_init_stage_alert},
    {WATCH_APP_INIT_STAGE_ALARM, watch_app_init_stage_alarm},
    {WATCH_APP_INIT_STAGE_WDT, watch_app_init_stage_wdt},
    {WATCH_APP_INIT_STAGE_BATTERY, watch_app_init_stage_battery},
    {WATCH_APP_INIT_STAGE_UI, watch_app_init_stage_ui},
};

static void watch_app_init_report_reset(WatchAppInitReport *report)
{
    if (report == NULL) {
        return;
    }
    memset(report, 0, sizeof(*report));
    report->failed_stage = WATCH_APP_INIT_STAGE_NONE;
    report->last_completed_stage = WATCH_APP_INIT_STAGE_NONE;
}

const char *watch_app_init_stage_name(WatchAppInitStage stage)
{
    return (stage < WATCH_APP_INIT_STAGE_COUNT) ? g_watch_app_init_stage_names[stage] : "UNKNOWN";
}

const char *watch_app_init_status_name(WatchAppInitStageStatus status)
{
    return (status <= WATCH_APP_INIT_STATUS_FAILED) ? g_watch_app_init_status_names[status] : "UNKNOWN";
}

bool watch_app_runtime_init_state_checked(WatchAppRuntimeState *state, WatchAppInitReport *out)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;
    size_t i;

    if (state == NULL) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    watch_app_reset_stage_telemetry(state->stage_telemetry,
                                    state->stage_history,
                                    &state->stage_history_head,
                                    &state->stage_history_count);
    watch_app_register_qos_providers(state);
    watch_app_init_report_reset(&state->init_report);

    for (i = 0U; i < (sizeof(g_watch_app_init_descriptors) / sizeof(g_watch_app_init_descriptors[0])); ++i) {
        const WatchAppInitDescriptor *descriptor = &g_watch_app_init_descriptors[i];
        WatchAppInitStageStatus status = descriptor->init_fn();

        state->init_report.stage_status[descriptor->stage] = status;
        if (status == WATCH_APP_INIT_STATUS_OK || status == WATCH_APP_INIT_STATUS_DEGRADED) {
            state->init_report.last_completed_stage = descriptor->stage;
        }
        if (status == WATCH_APP_INIT_STATUS_DEGRADED) {
            state->init_report.degraded = true;
        }
        if (status == WATCH_APP_INIT_STATUS_FAILED) {
            state->init_report.failed_stage = descriptor->stage;
            if (out != NULL) {
                *out = state->init_report;
            }
            return false;
        }

        if (descriptor->stage == WATCH_APP_INIT_STAGE_ACTIVITY) {
            if (model_get_domain_state(&domain_state) != NULL) {
                sensor_service_set_sensitivity(domain_state.settings.sensor_sensitivity);
                state->last_sensor_sensitivity = domain_state.settings.sensor_sensitivity;
            }
        }
        if (descriptor->stage == WATCH_APP_INIT_STAGE_ALARM) {
            if (model_get_runtime_state(&runtime_state) != NULL) {
                diag_service_note_boot(runtime_state.reset_reason);
            }
        }
        if (descriptor->stage == WATCH_APP_INIT_STAGE_BATTERY) {
            battery_service_sample_now();
        }
        if (descriptor->stage == WATCH_APP_INIT_STAGE_UI) {
            ui_request_render();
        }
    }

    state->last_storage_commit_ms = storage_service_get_last_commit_ms();
    state->last_key_scan = platform_time_now_ms();
    memset(&state->sleep_request, 0, sizeof(state->sleep_request));
    crash_capsule_boot_complete();
    state->init_report.success = true;

    if (out != NULL) {
        *out = state->init_report;
    }
    return true;
}

void watch_app_runtime_init_state(WatchAppRuntimeState *state)
{
    (void)watch_app_runtime_init_state_checked(state, NULL);
}


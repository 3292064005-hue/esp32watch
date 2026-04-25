#include "companion_proto_internal.h"
#include "companion_proto_contract.h"
#include "model.h"
#include "services/diag_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/time_service.h"
#include "watch_app.h"
#include "system_init.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *time_state_name;
    const char *reset_reason_name;
    const char *storage_backend_name;
    uint8_t storage_version;
} CompanionInfoExport;

typedef struct {
    uint8_t brightness;
    uint8_t watchface;
    uint32_t goal;
    bool auto_wake;
    bool auto_sleep;
    bool dnd;
    bool vibrate;
    bool show_seconds;
    bool animations;
    uint8_t sensor_sensitivity;
    uint8_t screen_timeout_idx;
} CompanionSettingsExport;

typedef struct {
    bool safe_mode_active;
    bool safe_mode_can_clear;
    const char *safe_mode_reason_name;
    const char *last_fault_name;
    uint8_t last_fault_count;
    uint16_t retained_checkpoint;
    uint32_t retained_max_loop_ms;
    uint8_t active_fault_count;
    uint8_t blocking_fault_count;
    uint32_t storage_fail_count;
    uint32_t sensor_fault_event_count;
    uint32_t display_tx_fail_count;
    uint32_t key_overflow_event_count;
    uint32_t ui_mutation_overflow_event_count;
    uint32_t battery_sample_count;
    uint32_t wdt_pet_count;
    bool previous_boot_incomplete;
    const char *previous_init_failed_stage_name;
} CompanionDiagExport;

typedef struct {
    bool online;
    bool calibrated;
    const char *runtime_state_name;
    const char *calibration_state_name;
    uint8_t quality;
    uint8_t reinit_count;
    uint8_t error_code;
    int16_t motion_score;
    bool wrist_raise;
} CompanionSensorExport;

typedef struct {
    const char *last_fault_name;
    uint8_t last_fault_count;
    uint16_t last_fault_value;
    uint8_t last_fault_aux;
} CompanionFaultExport;

typedef struct {
    uint32_t steps;
    uint32_t goal;
    uint16_t active_minutes;
    uint8_t activity_level;
    uint8_t motion_state;
    bool wrist_raise;
} CompanionActivityExport;

typedef struct {
    const char *backend_name;
    uint8_t version;
    bool crc_valid;
    uint16_t stored_crc;
    uint16_t calculated_crc;
    uint8_t pending_mask;
    uint32_t commit_count;
    uint32_t wear_count;
} CompanionStorageExport;

typedef struct {
    const char *source_name;
    uint32_t epoch;
    bool valid;
    const char *time_state_name;
} CompanionClockExport;

typedef struct {
    uint8_t history_count;
    const char *last_stage_name;
    const char *last_event_name;
    uint32_t last_timestamp_ms;
    uint16_t last_duration_ms;
    uint16_t last_budget_ms;
    uint16_t last_deferred_count;
} CompanionPerfExport;

size_t companion_proto_write_response(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    int n;

    if (out == NULL || out_size == 0U) {
        return 0U;
    }

    va_start(ap, fmt);
    n = vsnprintf(out, out_size, fmt, ap);
    va_end(ap);

    if (n < 0) {
        out[0] = '\0';
        return 0U;
    }
    if ((size_t)n >= out_size) {
        return out_size - 1U;
    }
    return (size_t)n;
}

static void build_info_export(CompanionInfoExport *out)
{
    ModelDomainState domain_state;
    ModelRuntimeState runtime_state;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (model_get_domain_state(&domain_state) == NULL || model_get_runtime_state(&runtime_state) == NULL) {
        out->time_state_name = "UNKNOWN";
        out->reset_reason_name = "UNKNOWN";
        out->storage_backend_name = "UNKNOWN";
        out->storage_version = 0U;
        return;
    }

    out->time_state_name = model_time_state_name(domain_state.time_state);
    out->reset_reason_name = model_reset_reason_name(runtime_state.reset_reason);
    out->storage_backend_name = storage_service_get_backend_name();
    out->storage_version = storage_service_get_version();
}

static void build_settings_export(CompanionSettingsExport *out)
{
    ModelDomainState domain_state;

    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }

    out->brightness = domain_state.settings.brightness;
    out->watchface = domain_state.settings.watchface;
    out->goal = domain_state.settings.goal;
    out->auto_wake = domain_state.settings.auto_wake;
    out->auto_sleep = domain_state.settings.auto_sleep;
    out->dnd = domain_state.settings.dnd;
    out->vibrate = domain_state.settings.vibrate;
    out->show_seconds = domain_state.settings.show_seconds;
    out->animations = domain_state.settings.animations;
    out->sensor_sensitivity = domain_state.settings.sensor_sensitivity;
    out->screen_timeout_idx = domain_state.settings.screen_timeout_idx;
}

static void build_diag_export(CompanionDiagExport *out)
{
    DiagFaultCode code = DIAG_FAULT_NONE;
    DiagFaultInfo info;
    DiagExportSnapshot snapshot;
    bool has_last_fault;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    has_last_fault = diag_service_get_last_fault(&code, &info);
    out->safe_mode_active = diag_service_is_safe_mode_active();
    out->safe_mode_can_clear = diag_service_can_clear_safe_mode();
    out->safe_mode_reason_name = diag_service_safe_mode_reason_name(diag_service_get_safe_mode_reason());
    out->last_fault_name = has_last_fault ? diag_service_fault_name(code) : "NONE";
    out->last_fault_count = has_last_fault ? info.count : 0U;
    out->retained_checkpoint = diag_service_get_last_retained_checkpoint();
    out->retained_max_loop_ms = diag_service_get_retained_max_loop_ms();
    out->previous_init_failed_stage_name = "NONE";

    if (diag_service_export_snapshot(&snapshot)) {
        out->active_fault_count = snapshot.active_fault_count;
        out->blocking_fault_count = snapshot.blocking_fault_count;
        out->storage_fail_count = snapshot.storage_fail_count;
        out->sensor_fault_event_count = snapshot.sensor_fault_event_count;
        out->display_tx_fail_count = snapshot.display_tx_fail_count;
        out->key_overflow_event_count = snapshot.key_overflow_event_count;
        out->ui_mutation_overflow_event_count = snapshot.ui_mutation_overflow_event_count;
        out->battery_sample_count = snapshot.battery_sample_count;
        out->wdt_pet_count = snapshot.wdt_pet_count;
        out->previous_boot_incomplete = snapshot.previous_boot_incomplete;
        out->previous_init_failed_stage_name = system_init_stage_name((SystemInitStage)snapshot.previous_init_failed_stage);
    }
}

static void build_sensor_export(CompanionSensorExport *out)
{
    const SensorSnapshot *snap = sensor_service_get_snapshot();
    ModelDomainState domain_state;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (snap != NULL) {
        out->online = snap->online;
        out->calibrated = snap->calibrated;
        out->runtime_state_name = sensor_runtime_state_name(snap->runtime_state);
        out->calibration_state_name = sensor_calibration_state_name(snap->calibration_state);
        out->quality = snap->quality;
        out->reinit_count = snap->reinit_count;
        out->error_code = snap->error_code;
        out->motion_score = snap->motion_score;
    } else {
        out->runtime_state_name = "UNINIT";
        out->calibration_state_name = "IDLE";
    }

    if (model_get_domain_state(&domain_state) != NULL) {
        out->wrist_raise = domain_state.activity.wrist_raise;
    }
}

static bool build_fault_export(CompanionFaultExport *out)
{
    DiagFaultCode code = DIAG_FAULT_NONE;
    DiagFaultInfo info;
    bool has_last_fault;

    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    has_last_fault = diag_service_get_last_fault(&code, &info);
    out->last_fault_name = has_last_fault ? diag_service_fault_name(code) : "NONE";
    out->last_fault_count = has_last_fault ? info.count : 0U;
    out->last_fault_value = has_last_fault ? info.last_value : 0U;
    out->last_fault_aux = has_last_fault ? info.last_aux : 0U;
    return has_last_fault;
}

static void build_activity_export(CompanionActivityExport *out)
{
    ModelDomainState domain_state;

    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (model_get_domain_state(&domain_state) == NULL) {
        return;
    }

    out->steps = domain_state.activity.steps;
    out->goal = domain_state.activity.goal;
    out->active_minutes = domain_state.activity.active_minutes;
    out->activity_level = domain_state.activity.activity_level;
    out->motion_state = domain_state.activity.motion_state;
    out->wrist_raise = domain_state.activity.wrist_raise;
}

static void build_storage_export(CompanionStorageExport *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->backend_name = storage_service_get_backend_name();
    out->version = storage_service_get_version();
    out->crc_valid = storage_service_is_crc_valid();
    out->stored_crc = storage_service_get_stored_crc();
    out->calculated_crc = storage_service_get_calculated_crc();
    out->pending_mask = storage_service_get_pending_mask();
    out->commit_count = storage_service_get_commit_count();
    out->wear_count = storage_service_get_wear_count();
}

static void build_clock_export(CompanionClockExport *out)
{
    TimeSourceSnapshot snapshot;
    ModelDomainState domain_state;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (!time_service_get_source_snapshot(&snapshot)) {
        out->source_name = "UNKNOWN";
        out->time_state_name = "UNKNOWN";
        return;
    }

    out->source_name = time_service_source_name(snapshot.source);
    out->epoch = snapshot.epoch;
    out->valid = snapshot.valid;
    if (model_get_domain_state(&domain_state) != NULL) {
        out->time_state_name = model_time_state_name(domain_state.time_state);
    } else {
        out->time_state_name = "UNKNOWN";
    }
}

static void build_perf_export(CompanionPerfExport *out)
{
    WatchAppStageHistoryEntry history;
    WatchAppStageTelemetry telemetry;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->history_count = watch_app_get_stage_history_count();
    out->last_stage_name = "NONE";
    out->last_event_name = "NONE";
    if (!watch_app_get_stage_history_recent(0U, &history)) {
        return;
    }

    out->last_stage_name = watch_app_stage_name(history.stage);
    out->last_event_name = history.deferred != 0U ? "DEFER" : (history.over_budget != 0U ? "OVER" : "OK");
    out->last_timestamp_ms = history.timestamp_ms;
    out->last_duration_ms = history.duration_ms;
    out->last_budget_ms = history.budget_ms;
    if (watch_app_get_stage_telemetry(history.stage, &telemetry)) {
        out->last_deferred_count = telemetry.deferred_count;
    }
}

size_t companion_proto_format_info(char *out, size_t out_size)
{
    CompanionInfoExport info;

    build_info_export(&info);
    return companion_proto_write_response(out, out_size,
                                          "OK profile=watch time=%s reset=%s backend=%s ver=%u",
                                          info.time_state_name,
                                          info.reset_reason_name,
                                          info.storage_backend_name,
                                          (unsigned)info.storage_version);
}

size_t companion_proto_format_settings(char *out, size_t out_size)
{
    CompanionSettingsExport settings;

    build_settings_export(&settings);
    return companion_proto_write_response(out, out_size,
                                          "OK bright=%u face=%u goal=%lu autowake=%u autosleep=%u dnd=%u vib=%u seconds=%u anim=%u sens=%u timeout=%u",
                                          (unsigned)settings.brightness,
                                          (unsigned)settings.watchface,
                                          (unsigned long)settings.goal,
                                          settings.auto_wake ? 1U : 0U,
                                          settings.auto_sleep ? 1U : 0U,
                                          settings.dnd ? 1U : 0U,
                                          settings.vibrate ? 1U : 0U,
                                          settings.show_seconds ? 1U : 0U,
                                          settings.animations ? 1U : 0U,
                                          (unsigned)settings.sensor_sensitivity,
                                          (unsigned)settings.screen_timeout_idx);
}

size_t companion_proto_format_diag(char *out, size_t out_size)
{
    CompanionDiagExport diag;

    build_diag_export(&diag);
    return companion_proto_write_response(out, out_size,
                                          "OK safe=%u clear=%u reason=%s lastfault=%s count=%u retaincp=%u maxloop=%lu active=%u blocking=%u storfail=%lu sensorfail=%lu dispfail=%lu keyovf=%lu uiqovf=%lu batsamp=%lu wdtpet=%lu prevboot=%u initfail=%s",
                                          diag.safe_mode_active ? 1U : 0U,
                                          diag.safe_mode_can_clear ? 1U : 0U,
                                          diag.safe_mode_reason_name,
                                          diag.last_fault_name,
                                          (unsigned)diag.last_fault_count,
                                          (unsigned)diag.retained_checkpoint,
                                          (unsigned long)diag.retained_max_loop_ms,
                                          (unsigned)diag.active_fault_count,
                                          (unsigned)diag.blocking_fault_count,
                                          (unsigned long)diag.storage_fail_count,
                                          (unsigned long)diag.sensor_fault_event_count,
                                          (unsigned long)diag.display_tx_fail_count,
                                          (unsigned long)diag.key_overflow_event_count,
                                          (unsigned long)diag.ui_mutation_overflow_event_count,
                                          (unsigned long)diag.battery_sample_count,
                                          (unsigned long)diag.wdt_pet_count,
                                          diag.previous_boot_incomplete ? 1U : 0U,
                                          diag.previous_init_failed_stage_name);
}

size_t companion_proto_format_sensor(char *out, size_t out_size)
{
    CompanionSensorExport sensor;

    build_sensor_export(&sensor);
    return companion_proto_write_response(out, out_size,
                                          "OK online=%u state=%s calstate=%s calib=%u qual=%u reinit=%u err=%u motion=%d wrist=%u",
                                          sensor.online ? 1U : 0U,
                                          sensor.runtime_state_name,
                                          sensor.calibration_state_name,
                                          sensor.calibrated ? 1U : 0U,
                                          (unsigned)sensor.quality,
                                          (unsigned)sensor.reinit_count,
                                          (unsigned)sensor.error_code,
                                          (int)sensor.motion_score,
                                          sensor.wrist_raise ? 1U : 0U);
}

size_t companion_proto_format_faults(char *out, size_t out_size)
{
    CompanionFaultExport faults;

    if (!build_fault_export(&faults)) {
        return companion_proto_write_response(out, out_size, "OK last=NONE count=0");
    }
    return companion_proto_write_response(out, out_size,
                                          "OK last=%s count=%u value=%u aux=%u",
                                          faults.last_fault_name,
                                          (unsigned)faults.last_fault_count,
                                          (unsigned)faults.last_fault_value,
                                          (unsigned)faults.last_fault_aux);
}

size_t companion_proto_format_activity(char *out, size_t out_size)
{
    CompanionActivityExport activity;

    build_activity_export(&activity);
    return companion_proto_write_response(out, out_size,
                                          "OK steps=%lu goal=%lu active_min=%u level=%u motion=%u wrist=%u",
                                          (unsigned long)activity.steps,
                                          (unsigned long)activity.goal,
                                          (unsigned)activity.active_minutes,
                                          (unsigned)activity.activity_level,
                                          (unsigned)activity.motion_state,
                                          activity.wrist_raise ? 1U : 0U);
}

size_t companion_proto_format_storage(char *out, size_t out_size)
{
    CompanionStorageExport storage;

    build_storage_export(&storage);
    return companion_proto_write_response(out, out_size,
                                          "OK backend=%s ver=%u crc=%u stored=%u calc=%u pending=%u commits=%lu wear=%lu",
                                          storage.backend_name,
                                          (unsigned)storage.version,
                                          storage.crc_valid ? 1U : 0U,
                                          (unsigned)storage.stored_crc,
                                          (unsigned)storage.calculated_crc,
                                          (unsigned)storage.pending_mask,
                                          (unsigned long)storage.commit_count,
                                          (unsigned long)storage.wear_count);
}

size_t companion_proto_format_clock(char *out, size_t out_size)
{
    CompanionClockExport clock_state;

    build_clock_export(&clock_state);
    return companion_proto_write_response(out,
                                          out_size,
                                          "OK source=%s epoch=%lu valid=%u time=%s",
                                          clock_state.source_name,
                                          (unsigned long)clock_state.epoch,
                                          clock_state.valid ? 1U : 0U,
                                          clock_state.time_state_name);
}

size_t companion_proto_format_perf(char *out, size_t out_size)
{
    CompanionPerfExport perf;

    build_perf_export(&perf);
    return companion_proto_write_response(out, out_size,
                                          "OK hist=%u stage=%s event=%s ts=%lu dur=%u budget=%u defercnt=%u",
                                          (unsigned)perf.history_count,
                                          perf.last_stage_name,
                                          perf.last_event_name,
                                          (unsigned long)perf.last_timestamp_ms,
                                          (unsigned)perf.last_duration_ms,
                                          (unsigned)perf.last_budget_ms,
                                          (unsigned)perf.last_deferred_count);
}

size_t companion_proto_format_proto(char *out, size_t out_size)
{
    return companion_proto_write_response(out,
                                          out_size,
                                          "OK proto=%u caps=%s",
                                          (unsigned)companion_proto_contract_version(),
                                          companion_proto_contract_caps_csv());
}

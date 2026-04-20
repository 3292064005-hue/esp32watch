#include "services/runtime_side_effect_service.h"

#include "app_limits.h"
#include "services/display_service.h"
#include "services/sensor_service.h"
#include "services/storage_facade.h"
#include "services/storage_service.h"
#include <string.h>

static void runtime_side_effect_report_init(RuntimeSideEffectReport *out,
                                            RuntimeSideEffectContext context,
                                            RuntimeSideEffectCommitPolicy commit_policy)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->context = context;
    out->commit_policy = commit_policy;
    out->commit_reason = STORAGE_COMMIT_REASON_NONE;
    out->commit_ok = true;
}

bool runtime_side_effect_service_apply_pending(RuntimeSideEffectContext context,
                                               RuntimeSideEffectCommitPolicy commit_policy,
                                               uint8_t *last_sensor_sensitivity,
                                               RuntimeSideEffectReport *out)
{
    RuntimeSideEffectReport local_report;
    RuntimeSideEffectReport *report = out != NULL ? out : &local_report;
    StorageCommitReason commit_reason = STORAGE_COMMIT_REASON_NONE;
    uint32_t flags = model_consume_runtime_requests(&commit_reason);
    ModelDomainState domain_state = {0};

    runtime_side_effect_report_init(report, context, commit_policy);
    report->consumed_flags = flags;
    report->commit_reason = commit_reason;
    if (flags == 0U) {
        return true;
    }

    report->domain_state_loaded = model_get_domain_state(&domain_state) != NULL;
    if (!report->domain_state_loaded) {
        report->commit_ok = false;
        return false;
    }

    if ((flags & MODEL_RUNTIME_REQUEST_APPLY_BRIGHTNESS) != 0U) {
        display_service_apply_settings(&domain_state.settings);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_SYNC_SENSOR_SETTINGS) != 0U) {
        sensor_service_set_sensitivity(domain_state.settings.sensor_sensitivity);
        if (last_sensor_sensitivity != NULL) {
            *last_sensor_sensitivity = domain_state.settings.sensor_sensitivity;
        }
    }

    if ((flags & MODEL_RUNTIME_REQUEST_CLEAR_SENSOR_CALIBRATION) != 0U) {
        sensor_service_clear_calibration();
        storage_facade_clear_sensor_calibration();
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STAGE_SETTINGS) != 0U) {
        storage_facade_save_settings(&domain_state.settings);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STAGE_ALARMS) != 0U) {
        storage_facade_save_alarms(domain_state.alarms, APP_MAX_ALARMS);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STAGE_GAME_STATS) != 0U) {
        storage_facade_save_game_stats(&domain_state.game_stats);
    }

    if ((flags & MODEL_RUNTIME_REQUEST_STORAGE_COMMIT) != 0U) {
        report->commit_requested = true;
        storage_service_request_commit(commit_reason);
        if (commit_policy == RUNTIME_SIDE_EFFECT_COMMIT_IMMEDIATE) {
            report->commit_executed = true;
            storage_service_commit_now_with_reason(commit_reason);
            report->commit_ok = storage_service_get_last_commit_ok();
            return report->commit_ok;
        }
    }

    report->commit_ok = true;
    return true;
}

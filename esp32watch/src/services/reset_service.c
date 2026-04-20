#include "services/reset_service.h"
#include "services/device_config_authority.h"
#include "services/display_service.h"
#include "services/runtime_event_service.h"
#include "services/sensor_service.h"
#include "services/storage_facade.h"
#include "services/storage_service.h"
#include "services/runtime_side_effect_service.h"
#include "model.h"
#include <string.h>

static bool reset_service_notify(RuntimeServiceEvent event)
{
    return runtime_event_service_handler_count() > 0U && runtime_event_service_publish_notify(event);
}

static void reset_report_init(ResetActionReport *out, ResetActionKind kind)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->kind = kind;
}

static bool reset_service_stage_and_commit_app_state(RuntimeSideEffectContext context)
{
    return runtime_side_effect_service_apply_pending(context,
                                                     RUNTIME_SIDE_EFFECT_COMMIT_IMMEDIATE,
                                                     NULL,
                                                     NULL);
}

bool reset_service_reset_app_state(ResetActionReport *out)
{
    bool ok;

    reset_report_init(out, RESET_ACTION_APP_STATE);
    model_restore_defaults();
    ok = reset_service_stage_and_commit_app_state(RUNTIME_SIDE_EFFECT_CONTEXT_RESET);
    if (!ok) {
        return false;
    }
    if (out != NULL) {
        out->app_state_reset = true;
        out->audit_event_dispatched = reset_service_notify(RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED);
        out->audit_notified = out->audit_event_dispatched;
    } else {
        (void)reset_service_notify(RUNTIME_SERVICE_EVENT_RESET_APP_STATE_COMPLETED);
    }
    return true;
}

bool reset_service_reset_device_config(ResetActionReport *out)
{
    DeviceConfigAuthorityApplyReport apply = {0};
    bool ok;

    reset_report_init(out, RESET_ACTION_DEVICE_CONFIG);
    ok = device_config_authority_restore_defaults(&apply);
    if (!apply.committed) {
        return false;
    }

    if (out != NULL) {
        out->device_config_reset = true;
        out->runtime_reload_requested = apply.runtime_reload_requested;
        out->runtime_reload_ok = apply.runtime_reload_ok;
        out->reload = apply.reload;
        out->audit_event_dispatched = reset_service_notify(RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED);
        out->audit_notified = out->audit_event_dispatched;
    } else {
        (void)reset_service_notify(RUNTIME_SERVICE_EVENT_RESET_DEVICE_CONFIG_COMPLETED);
    }
    return ok;
}

bool reset_service_factory_reset(ResetActionReport *out)
{
    DeviceConfigAuthorityApplyReport apply = {0};
    bool ok;

    reset_report_init(out, RESET_ACTION_FACTORY);
    ok = device_config_authority_restore_defaults(&apply);
    if (!apply.committed) {
        return false;
    }

    model_factory_reset_defaults();
    if (!reset_service_stage_and_commit_app_state(RUNTIME_SIDE_EFFECT_CONTEXT_FACTORY)) {
        return false;
    }
    if (out != NULL) {
        out->app_state_reset = true;
        out->device_config_reset = true;
        out->runtime_reload_requested = apply.runtime_reload_requested;
        out->runtime_reload_ok = apply.runtime_reload_ok;
        out->reload = apply.reload;
        out->audit_event_dispatched = reset_service_notify(RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED);
        out->audit_notified = out->audit_event_dispatched;
    } else {
        (void)reset_service_notify(RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED);
    }
    return ok;
}

#include "ui_system_read_model.h"
#include "key.h"
#include "sensor.h"
#include "services/diag_service.h"
#include "services/input_service.h"
#include "services/storage_service.h"
#include "services/wdt_service.h"
#include <string.h>

void ui_system_read_model_capture(UiSystemReadModel *out,
                                  const ModelRuntimeState *runtime_state,
                                  uint32_t now_ms)
{
    DiagFaultCode code = DIAG_FAULT_NONE;
    DiagFaultInfo info;
    DiagLogEntry last_log;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    if (runtime_state == NULL) {
        return;
    }
    out->sensor_runtime_state_name = sensor_runtime_state_name((SensorRuntimeState)runtime_state->sensor.runtime_state);
    out->sensor_calibration_state_name = sensor_calibration_state_name((SensorCalibrationState)runtime_state->sensor.calibration_state);
    out->sensor_retry_backoff_s = runtime_state->sensor.retry_backoff_ms / 1000UL;
    out->sensor_offline_elapsed_s = (runtime_state->sensor.offline_since_ms == 0U || now_ms < runtime_state->sensor.offline_since_ms) ?
        0U : ((now_ms - runtime_state->sensor.offline_since_ms) / 1000UL);

    out->storage_backend_name = storage_service_get_backend_name();
    out->storage_commit_state_name = storage_service_commit_state_name(storage_service_get_commit_state());
    out->storage_transaction_active = storage_service_is_transaction_active();
    out->storage_sleep_flush_pending = storage_service_is_sleep_flush_pending();
    out->storage_wear_count = storage_service_get_wear_count();

    out->safe_mode_active = diag_service_is_safe_mode_active();
    out->safe_mode_can_clear = diag_service_can_clear_safe_mode();
    out->safe_mode_reason_name = diag_service_safe_mode_reason_name(diag_service_get_safe_mode_reason());
    out->has_last_fault = diag_service_get_last_fault(&code, &info);
    out->last_fault_name = out->has_last_fault ? diag_service_fault_name(code) : "NONE";
    out->last_fault_severity_name = out->has_last_fault ? diag_service_fault_severity_name(info.severity) : "NONE";
    out->last_fault_count = out->has_last_fault ? info.count : 0U;
    out->last_fault_value = out->has_last_fault ? info.last_value : 0U;
    out->last_fault_aux = out->has_last_fault ? info.last_aux : 0U;
    out->retained_checkpoint = (uint8_t)diag_service_get_last_retained_checkpoint();
    out->retained_max_loop_ms = diag_service_get_retained_max_loop_ms();

    out->wdt_last_checkpoint_name = wdt_service_checkpoint_name(wdt_service_get_last_checkpoint());
    out->wdt_last_checkpoint_result_name = wdt_service_checkpoint_result_name(wdt_service_get_last_checkpoint_result());
    out->wdt_max_loop_ms = wdt_service_get_max_loop_ms();

    out->has_last_log = diag_service_get_last_log(&last_log);
    out->last_log_name = out->has_last_log ? diag_service_event_name((DiagEventCode)last_log.code) : "NONE";
    out->last_log_value = out->has_last_log ? last_log.value : 0U;
    out->last_log_aux = out->has_last_log ? last_log.aux : 0U;
    out->display_tx_fail_count = diag_service_get_display_tx_fail_count();
    out->ui_mutation_overflow_event_count = diag_service_get_ui_mutation_overflow_event_count();
    out->key_up_down = input_service_is_down(KEY_ID_UP);
    out->key_down_down = input_service_is_down(KEY_ID_DOWN);
    out->key_ok_down = input_service_is_down(KEY_ID_OK);
    out->key_back_down = input_service_is_down(KEY_ID_BACK);
}

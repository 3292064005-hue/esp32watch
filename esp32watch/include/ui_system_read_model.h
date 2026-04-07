#ifndef UI_SYSTEM_READ_MODEL_H
#define UI_SYSTEM_READ_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *sensor_runtime_state_name;
    const char *sensor_calibration_state_name;
    uint32_t sensor_retry_backoff_s;
    uint32_t sensor_offline_elapsed_s;
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
} UiSystemReadModel;

/**
 * @brief Capture a service-backed read model for UI snapshot assembly.
 *
 * @param[out] out Destination read model. When NULL the call is ignored.
 * @param[in] runtime_state Current runtime model snapshot. When NULL, @p out is zeroed and the call returns.
 * @param[in] now_ms Monotonic tick used for elapsed-time calculations.
 * @return void
 * @throws None.
 * @note This isolates presenter-facing snapshot assembly from direct service fan-out.
 *       On invalid input the function leaves callers with a deterministic zero-value snapshot.
 */
void ui_system_read_model_capture(UiSystemReadModel *out,
                                  const ModelRuntimeState *runtime_state,
                                  uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

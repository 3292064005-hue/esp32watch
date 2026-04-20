#ifndef SERVICES_RUNTIME_SIDE_EFFECT_SERVICE_H
#define SERVICES_RUNTIME_SIDE_EFFECT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RUNTIME_SIDE_EFFECT_CONTEXT_NORMAL = 0,
    RUNTIME_SIDE_EFFECT_CONTEXT_RESET,
    RUNTIME_SIDE_EFFECT_CONTEXT_FACTORY
} RuntimeSideEffectContext;

typedef enum {
    RUNTIME_SIDE_EFFECT_COMMIT_DEFERRED = 0,
    RUNTIME_SIDE_EFFECT_COMMIT_IMMEDIATE
} RuntimeSideEffectCommitPolicy;

typedef struct {
    RuntimeSideEffectContext context;
    RuntimeSideEffectCommitPolicy commit_policy;
    uint32_t consumed_flags;
    StorageCommitReason commit_reason;
    bool domain_state_loaded;
    bool commit_requested;
    bool commit_executed;
    bool commit_ok;
} RuntimeSideEffectReport;

/**
 * @brief Consume pending model runtime requests and apply all associated side effects through a single authority path.
 *
 * This service is the only runtime-owned executor that is allowed to translate
 * pending model runtime request flags into concrete display, sensor, staging,
 * and storage side effects. Callers choose the execution context and the commit
 * policy, but they do not duplicate the request-handling logic.
 *
 * @param[in] context Execution context used for diagnostics/documentation and for future policy branching.
 * @param[in] commit_policy Deferred policy only stages a storage commit request; immediate policy also forces the durable commit synchronously.
 * @param[in,out] last_sensor_sensitivity Optional mirror updated when the pending request reapplies sensor sensitivity.
 * @param[out] out Optional execution report.
 * @return true when pending requests were applied successfully or when no request was pending; false when the model domain state could not be loaded or an immediate commit failed.
 * @throws None.
 * @boundary_behavior When no runtime request is pending the function returns true, reports zero consumed flags, and performs no side effects. Immediate commit policy always issues request_commit before forcing commit_now so storage observers retain the queued reason.
 */
bool runtime_side_effect_service_apply_pending(RuntimeSideEffectContext context,
                                               RuntimeSideEffectCommitPolicy commit_policy,
                                               uint8_t *last_sensor_sensitivity,
                                               RuntimeSideEffectReport *out);

#ifdef __cplusplus
}
#endif

#endif

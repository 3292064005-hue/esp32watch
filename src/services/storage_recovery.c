#include "services/storage_service_internal.h"
#include "app_config.h"
#include "crash_capsule.h"
#include "persist.h"
#include "persist_flash.h"

bool storage_recovery_verify_runtime_state(StorageServiceState *state)
{
    bool ok;
    uint16_t stored_crc;
    uint16_t calc_crc;

    if (state == NULL) {
        return false;
    }

    ok = storage_backend_adapter_is_initialized();
    stored_crc = storage_backend_adapter_get_stored_crc();
    calc_crc = storage_backend_adapter_get_calculated_crc();

    if (ok && stored_crc != calc_crc) {
        ok = false;
    }
    if (ok && storage_backend_adapter_get_version() != APP_STORAGE_VERSION) {
        ok = false;
    }

    state->verify_failed = !ok;
    state->backend_degraded = !storage_backend_adapter_is_initialized();
    return ok;
}

bool storage_recovery_refresh_backend_state(StorageServiceState *state)
{
#if APP_STORAGE_USE_FLASH
    persist_flash_init();
#endif
    if (state != NULL) {
        storage_backend_adapter_capture_shadows(state);
        state->last_commit_ok = storage_recovery_verify_runtime_state(state);
        crash_capsule_note_storage(state->commit_count, storage_scheduler_pending_mask(state));
        return state->last_commit_ok;
    }
    return storage_backend_adapter_is_initialized();
}

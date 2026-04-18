#ifndef CRASH_CAPSULE_H
#define CRASH_CAPSULE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CRASH_TRAP_NONE = 0,
    CRASH_TRAP_NMI = 1,
    CRASH_TRAP_HARDFAULT = 2,
    CRASH_TRAP_MEMMANAGE = 3,
    CRASH_TRAP_BUSFAULT = 4,
    CRASH_TRAP_USAGEFAULT = 5,
    CRASH_TRAP_ERROR_HANDLER = 6
} CrashTrapId;

typedef struct {
    uint32_t magic;
    uint32_t boot_count;
    uint32_t last_tick_ms;
    uint32_t loop_start_ms;
    uint32_t max_loop_ms;
    uint32_t last_commit_count;
    uint8_t boot_in_progress;
    uint8_t reset_reason;
    uint8_t last_checkpoint;
    uint8_t last_checkpoint_result;
    uint8_t last_fault_code;
    uint8_t last_fault_severity;
    uint16_t last_fault_value;
    uint8_t last_fault_aux;
    uint8_t last_sensor_error;
    uint8_t last_sensor_reinit_count;
    uint8_t last_storage_pending_mask;
    uint8_t init_failed_stage;
    uint8_t init_failed_policy;
    uint8_t consecutive_incomplete_boots;
} CrashCapsuleSnapshot;

void crash_capsule_init(ResetReason reset_reason);
void crash_capsule_note_init_failure(uint8_t stage, uint8_t policy);
void crash_capsule_boot_complete(void);
void crash_capsule_note_checkpoint(uint8_t checkpoint, uint32_t now_ms);
void crash_capsule_note_checkpoint_result(uint8_t checkpoint, uint8_t result, uint32_t now_ms);
void crash_capsule_note_fault(uint8_t fault_code, uint16_t value, uint8_t aux);
void crash_capsule_note_fault_with_severity(uint8_t fault_code, uint16_t value, uint8_t aux, uint8_t severity);
void crash_capsule_note_trap(CrashTrapId trap_id, uint16_t fault_value, uint8_t fault_aux);
void crash_capsule_note_storage(uint32_t commit_count, uint8_t pending_mask);
void crash_capsule_note_sensor(uint8_t error_code, uint8_t reinit_count);
void crash_capsule_note_loop_start(uint32_t now_ms);
const CrashCapsuleSnapshot *crash_capsule_get_current(void);
const CrashCapsuleSnapshot *crash_capsule_get_previous(void);
bool crash_capsule_has_previous(void);
uint8_t crash_capsule_consecutive_incomplete_boots(void);

#ifdef __cplusplus
}
#endif

#endif

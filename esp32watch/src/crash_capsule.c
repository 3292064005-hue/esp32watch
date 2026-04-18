#include "crash_capsule.h"
#include <string.h>

#define CRASH_CAPSULE_MAGIC 0x43524153UL

#if defined(__GNUC__)
__attribute__((section(".noinit"))) static CrashCapsuleSnapshot g_retained_capsule;
#else
static CrashCapsuleSnapshot g_retained_capsule;
#endif

static CrashCapsuleSnapshot g_current_capsule;
static CrashCapsuleSnapshot g_previous_capsule;
static bool g_has_previous;

void crash_capsule_init(ResetReason reset_reason)
{
    if (g_current_capsule.magic == CRASH_CAPSULE_MAGIC &&
        g_current_capsule.boot_in_progress == 1U &&
        g_current_capsule.reset_reason == (uint8_t)reset_reason) {
        return;
    }

    if (g_retained_capsule.magic == CRASH_CAPSULE_MAGIC) {
        g_previous_capsule = g_retained_capsule;
        g_has_previous = true;
    } else {
        memset(&g_previous_capsule, 0, sizeof(g_previous_capsule));
        g_has_previous = false;
    }

    memset(&g_current_capsule, 0, sizeof(g_current_capsule));
    g_current_capsule.magic = CRASH_CAPSULE_MAGIC;
    g_current_capsule.boot_count = g_has_previous ? (g_previous_capsule.boot_count + 1U) : 1U;
    g_current_capsule.consecutive_incomplete_boots = (uint8_t)((g_has_previous && g_previous_capsule.boot_in_progress != 0U) ? (g_previous_capsule.consecutive_incomplete_boots + 1U) : 0U);
    g_current_capsule.boot_in_progress = 1U;
    g_current_capsule.reset_reason = (uint8_t)reset_reason;
    g_retained_capsule = g_current_capsule;
}

void crash_capsule_boot_complete(void)
{
    g_current_capsule.boot_in_progress = 0U;
    g_retained_capsule = g_current_capsule;
}

void crash_capsule_note_checkpoint(uint8_t checkpoint, uint32_t now_ms)
{
    crash_capsule_note_checkpoint_result(checkpoint, 0U, now_ms);
}

void crash_capsule_note_checkpoint_result(uint8_t checkpoint, uint8_t result, uint32_t now_ms)
{
    g_current_capsule.last_checkpoint = checkpoint;
    g_current_capsule.last_checkpoint_result = result;
    g_current_capsule.last_tick_ms = now_ms;
    if (g_current_capsule.loop_start_ms != 0U && now_ms >= g_current_capsule.loop_start_ms) {
        uint32_t loop_ms = now_ms - g_current_capsule.loop_start_ms;
        if (loop_ms > g_current_capsule.max_loop_ms) {
            g_current_capsule.max_loop_ms = loop_ms;
        }
    }
    g_retained_capsule = g_current_capsule;
}

void crash_capsule_note_fault(uint8_t fault_code, uint16_t value, uint8_t aux)
{
    crash_capsule_note_fault_with_severity(fault_code, value, aux, 0U);
}

void crash_capsule_note_fault_with_severity(uint8_t fault_code, uint16_t value, uint8_t aux, uint8_t severity)
{
    g_current_capsule.last_fault_code = fault_code;
    g_current_capsule.last_fault_value = value;
    g_current_capsule.last_fault_aux = aux;
    g_current_capsule.last_fault_severity = severity;
    g_retained_capsule = g_current_capsule;
}

void crash_capsule_note_trap(CrashTrapId trap_id, uint16_t fault_value, uint8_t fault_aux)
{
    crash_capsule_note_fault_with_severity((uint8_t)trap_id, fault_value, fault_aux, 0xFFU);
}

void crash_capsule_note_storage(uint32_t commit_count, uint8_t pending_mask)
{
    g_current_capsule.last_commit_count = commit_count;
    g_current_capsule.last_storage_pending_mask = pending_mask;
    g_retained_capsule = g_current_capsule;
}

void crash_capsule_note_sensor(uint8_t error_code, uint8_t reinit_count)
{
    g_current_capsule.last_sensor_error = error_code;
    g_current_capsule.last_sensor_reinit_count = reinit_count;
    g_retained_capsule = g_current_capsule;
}

void crash_capsule_note_loop_start(uint32_t now_ms)
{
    g_current_capsule.loop_start_ms = now_ms;
    g_current_capsule.last_tick_ms = now_ms;
    g_retained_capsule = g_current_capsule;
}

const CrashCapsuleSnapshot *crash_capsule_get_current(void)
{
    return &g_current_capsule;
}

const CrashCapsuleSnapshot *crash_capsule_get_previous(void)
{
    return &g_previous_capsule;
}

bool crash_capsule_has_previous(void)
{
    return g_has_previous;
}

uint8_t crash_capsule_consecutive_incomplete_boots(void)
{
    return g_current_capsule.consecutive_incomplete_boots;
}

void crash_capsule_note_init_failure(uint8_t stage, uint8_t policy)
{
    g_current_capsule.init_failed_stage = stage;
    g_current_capsule.init_failed_policy = policy;
    g_retained_capsule = g_current_capsule;
}

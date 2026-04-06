#ifndef DIAG_SERVICE_INTERNAL_H
#define DIAG_SERVICE_INTERNAL_H

#include "services/diag_service.h"
#include "common/ringbuf.h"

#define DIAG_LOG_CAPACITY 16U

typedef struct {
    RingBuf rb;
    DiagLogEntry storage[DIAG_LOG_CAPACITY];
    DiagLogEntry last;
    bool has_last;
    uint32_t storage_fail_count;
    uint32_t sensor_fault_event_count;
    uint32_t display_tx_fail_count;
    uint32_t key_overflow_event_count;
    uint32_t ui_mutation_overflow_event_count;
    uint32_t battery_sample_count;
    uint32_t wdt_pet_count;
    DiagFaultInfo faults[DIAG_FAULT_WDT_RESET + 1U];
    DiagFaultCode last_fault_code;
    bool has_last_fault;
    bool safe_mode_active;
    bool safe_mode_clear_allowed;
    uint32_t safe_mode_since_ms;
    DiagSafeModeReason safe_mode_reason;
} DiagServiceState;

extern DiagServiceState g_diag;

#endif

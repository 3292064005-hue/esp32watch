#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

void power_service_init(void);
void power_service_apply_settings(const SettingsState *settings);
void power_service_set_sleeping(bool sleeping);
void power_service_request_sleep(SleepReason reason);
bool power_service_is_sleeping(void);
void power_service_wake(void);
void power_service_wake_with_reason(WakeReason reason);
void power_service_toggle_sleep(void);
WakeReason power_service_get_last_wake_reason(void);
SleepReason power_service_get_last_sleep_reason(void);
uint32_t power_service_get_last_wake_ms(void);
uint32_t power_service_get_last_sleep_ms(void);
uint32_t power_service_get_last_idle_budget_ms(void);
bool power_service_last_idle_sleep_ok(void);
uint32_t power_service_get_idle_sleep_attempt_count(void);
uint32_t power_service_get_idle_sleep_reject_count(void);
void power_service_idle(bool can_sleep_cpu);

#endif

#ifndef POWER_POLICY_H
#define POWER_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"
#include "ui_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POWER_SLEEP_BLOCKER_NONE = 0,
    POWER_SLEEP_BLOCKER_PAGE,
    POWER_SLEEP_BLOCKER_EDIT,
    POWER_SLEEP_BLOCKER_POPUP,
    POWER_SLEEP_BLOCKER_TIMER,
    POWER_SLEEP_BLOCKER_STOPWATCH,
    POWER_SLEEP_BLOCKER_ALARM,
    POWER_SLEEP_BLOCKER_STORAGE,
    POWER_SLEEP_BLOCKER_SAFE_MODE
} PowerSleepBlocker;

typedef enum {
    POWER_QOS_NONE = 0,
    POWER_QOS_RENDER_DUE = 1 << 0,
    POWER_QOS_STORAGE_PENDING = 1 << 1,
    POWER_QOS_TRANSACTION_ACTIVE = 1 << 2,
    POWER_QOS_SAFE_MODE_ACTIVE = 1 << 3,
    POWER_QOS_SENSOR_BACKOFF_ACTIVE = 1 << 4,
    POWER_QOS_UI_FEEDBACK_PENDING = 1 << 5,
    POWER_QOS_ALARM_ACTIVE = 1 << 6,
    POWER_QOS_NETWORK_BUSY = 1 << 7,
    POWER_QOS_WEB_BUSY = 1 << 8
} PowerQosFlags;

typedef struct {
    bool render_due;
    bool storage_pending;
    bool transaction_active;
    bool safe_mode_active;
    bool sensor_backoff_active;
    bool ui_feedback_pending;
    bool alarm_active;
    bool network_busy;
    bool web_busy;
} PowerQosSnapshot;

typedef bool (*PowerQosProviderFn)(void *ctx, PowerQosSnapshot *snapshot);

void power_policy_reset_qos_registry(void);
bool power_policy_register_qos_provider(PowerQosProviderFn provider, void *ctx);
void power_policy_collect_qos_snapshot(PowerQosSnapshot *snapshot);

bool power_policy_can_auto_sleep(PageId page,
                                 bool editing,
                                 PopupType popup,
                                 bool timer_running,
                                 bool stopwatch_running,
                                 bool alarm_ringing,
                                 bool storage_pending,
                                 bool safe_mode_active,
                                 PowerSleepBlocker *blocker_out);

bool power_policy_can_enter_cpu_idle(bool render_due,
                                     bool storage_pending,
                                     bool transaction_active);
uint32_t power_policy_build_qos_mask(const PowerQosSnapshot *snapshot);
bool power_policy_can_enter_cpu_idle_ex(const PowerQosSnapshot *snapshot, uint32_t *qos_mask_out);

const char *power_policy_sleep_blocker_name(PowerSleepBlocker blocker);

#ifdef __cplusplus
}
#endif

#endif

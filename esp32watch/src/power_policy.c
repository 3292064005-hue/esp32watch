#include <stddef.h>
#include <string.h>
#include "power_policy.h"
#include "ui_page_registry.h"
#include "platform_api.h"

#define POWER_QOS_PROVIDER_MAX 8U

typedef struct {
    PowerQosProviderFn provider;
    void *ctx;
} PowerQosProviderEntry;

static PowerQosProviderEntry g_qos_providers[POWER_QOS_PROVIDER_MAX];

void power_policy_reset_qos_registry(void)
{
    memset(g_qos_providers, 0, sizeof(g_qos_providers));
}

bool power_policy_register_qos_provider(PowerQosProviderFn provider, void *ctx)
{
    size_t i;

    if (provider == NULL) {
        return false;
    }
    for (i = 0U; i < POWER_QOS_PROVIDER_MAX; ++i) {
        if (g_qos_providers[i].provider == NULL) {
            g_qos_providers[i].provider = provider;
            g_qos_providers[i].ctx = ctx;
            return true;
        }
    }
    return false;
}

void power_policy_collect_qos_snapshot(PowerQosSnapshot *snapshot)
{
    size_t i;

    if (snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    for (i = 0U; i < POWER_QOS_PROVIDER_MAX; ++i) {
        if (g_qos_providers[i].provider != NULL) {
            (void)g_qos_providers[i].provider(g_qos_providers[i].ctx, snapshot);
        }
    }
}

bool power_policy_can_auto_sleep(PageId page,
                                 bool editing,
                                 PopupType popup,
                                 bool timer_running,
                                 bool stopwatch_running,
                                 bool alarm_ringing,
                                 bool storage_pending,
                                 bool safe_mode_active,
                                 PowerSleepBlocker *blocker_out)
{
    PowerSleepBlocker blocker = POWER_SLEEP_BLOCKER_NONE;

    if (!ui_page_registry_allows_auto_sleep(page)) {
        blocker = POWER_SLEEP_BLOCKER_PAGE;
    } else if (editing) {
        blocker = POWER_SLEEP_BLOCKER_EDIT;
    } else if (popup != POPUP_NONE) {
        blocker = POWER_SLEEP_BLOCKER_POPUP;
    } else if (timer_running) {
        blocker = POWER_SLEEP_BLOCKER_TIMER;
    } else if (stopwatch_running) {
        blocker = POWER_SLEEP_BLOCKER_STOPWATCH;
    } else if (alarm_ringing) {
        blocker = POWER_SLEEP_BLOCKER_ALARM;
    } else if (storage_pending) {
        blocker = POWER_SLEEP_BLOCKER_STORAGE;
    } else if (safe_mode_active) {
        blocker = POWER_SLEEP_BLOCKER_SAFE_MODE;
    }

    if (blocker_out != 0) {
        *blocker_out = blocker;
    }
    return blocker == POWER_SLEEP_BLOCKER_NONE;
}

uint32_t power_policy_build_qos_mask(const PowerQosSnapshot *snapshot)
{
    uint32_t mask = POWER_QOS_NONE;

    if (snapshot == NULL) {
        return POWER_QOS_RENDER_DUE | POWER_QOS_STORAGE_PENDING | POWER_QOS_TRANSACTION_ACTIVE;
    }
    if (snapshot->render_due) {
        mask |= POWER_QOS_RENDER_DUE;
    }
    if (snapshot->storage_pending) {
        mask |= POWER_QOS_STORAGE_PENDING;
    }
    if (snapshot->transaction_active) {
        mask |= POWER_QOS_TRANSACTION_ACTIVE;
    }
    if (snapshot->safe_mode_active) {
        mask |= POWER_QOS_SAFE_MODE_ACTIVE;
    }
    if (snapshot->sensor_backoff_active) {
        mask |= POWER_QOS_SENSOR_BACKOFF_ACTIVE;
    }
    if (snapshot->ui_feedback_pending) {
        mask |= POWER_QOS_UI_FEEDBACK_PENDING;
    }
    if (snapshot->alarm_active) {
        mask |= POWER_QOS_ALARM_ACTIVE;
    }
    if (snapshot->network_busy) {
        mask |= POWER_QOS_NETWORK_BUSY;
    }
    if (snapshot->web_busy) {
        mask |= POWER_QOS_WEB_BUSY;
    }
    return mask;
}


static bool power_policy_qos_mask_needs_platform_pm_lock(uint32_t qos_mask)
{
    return qos_mask != POWER_QOS_NONE;
}

bool power_policy_platform_pm_lock_enter(uint32_t qos_mask, const char *owner)
{
    if (!power_policy_qos_mask_needs_platform_pm_lock(qos_mask)) {
        return true;
    }
    if (!platform_pm_lock_supported()) {
        return true;
    }
    return platform_pm_lock_acquire(owner != NULL ? owner : "runtime");
}

bool power_policy_platform_pm_lock_exit(uint32_t qos_mask, const char *owner)
{
    if (!power_policy_qos_mask_needs_platform_pm_lock(qos_mask)) {
        return true;
    }
    if (!platform_pm_lock_supported()) {
        return true;
    }
    return platform_pm_lock_release(owner != NULL ? owner : "runtime");
}

bool power_policy_can_enter_cpu_idle_ex(const PowerQosSnapshot *snapshot, uint32_t *qos_mask_out)
{
    PowerQosSnapshot collected;
    uint32_t mask;

    if (snapshot == NULL) {
        power_policy_collect_qos_snapshot(&collected);
        snapshot = &collected;
    }
    mask = power_policy_build_qos_mask(snapshot);

    if (qos_mask_out != NULL) {
        *qos_mask_out = mask;
    }
    return mask == POWER_QOS_NONE;
}

bool power_policy_can_enter_cpu_idle(bool render_due,
                                     bool storage_pending,
                                     bool transaction_active)
{
    PowerQosSnapshot snapshot;

    snapshot.render_due = render_due;
    snapshot.storage_pending = storage_pending;
    snapshot.transaction_active = transaction_active;
    snapshot.safe_mode_active = false;
    snapshot.sensor_backoff_active = false;
    snapshot.ui_feedback_pending = false;
    snapshot.alarm_active = false;
    snapshot.network_busy = false;
    snapshot.web_busy = false;
    return power_policy_can_enter_cpu_idle_ex(&snapshot, NULL);
}

const char *power_policy_sleep_blocker_name(PowerSleepBlocker blocker)
{
    switch (blocker) {
        case POWER_SLEEP_BLOCKER_PAGE: return "PAGE";
        case POWER_SLEEP_BLOCKER_EDIT: return "EDIT";
        case POWER_SLEEP_BLOCKER_POPUP: return "POPUP";
        case POWER_SLEEP_BLOCKER_TIMER: return "TIMER";
        case POWER_SLEEP_BLOCKER_STOPWATCH: return "SW";
        case POWER_SLEEP_BLOCKER_ALARM: return "ALARM";
        case POWER_SLEEP_BLOCKER_STORAGE: return "STORE";
        case POWER_SLEEP_BLOCKER_SAFE_MODE: return "SAFE";
        default: return "NONE";
    }
}

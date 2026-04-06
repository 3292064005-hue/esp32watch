#include "services/alarm_service.h"
#include "model_internal.h"
#include <stddef.h>

#define ALARM_EVAL_LOOKBACK_MAX_S 7200UL

static uint32_t g_last_eval_epoch;
static bool g_alarm_service_initialized;

static uint8_t weekday_to_mask_bit(uint8_t weekday)
{
    return (uint8_t)(1U << (weekday % 7U));
}

static bool alarm_weekday_matches(const AlarmState *alarm, uint8_t weekday)
{
    if (alarm->repeat_mask == 0U) {
        return true;
    }
    return (alarm->repeat_mask & weekday_to_mask_bit(weekday)) != 0U;
}

static bool alarm_due_in_window(const AlarmState *alarm, uint32_t start_exclusive, uint32_t end_inclusive, uint16_t *due_day_id)
{
    uint32_t minute_epoch;
    DateTime dt;

    if (end_inclusive <= start_exclusive) {
        return false;
    }

    minute_epoch = ((start_exclusive / 60UL) + 1UL) * 60UL;
    while (minute_epoch <= end_inclusive) {
        model_epoch_to_datetime(minute_epoch, &dt);
        if (dt.hour == alarm->hour && dt.minute == alarm->minute && alarm_weekday_matches(alarm, dt.weekday)) {
            *due_day_id = (uint16_t)(minute_epoch / 86400UL);
            if (alarm->last_trigger_day != *due_day_id) {
                return true;
            }
        }
        minute_epoch += 60UL;
    }

    return false;
}

void alarm_service_init(void)
{
    ModelDomainState domain_state;
    uint32_t epoch;

    g_alarm_service_initialized = true;
    if (model_get_domain_state(&domain_state) == NULL || !domain_state.time_valid) {
        g_last_eval_epoch = 0U;
        return;
    }

    epoch = model_datetime_to_epoch(&domain_state.now);
    g_last_eval_epoch = epoch > 60UL ? (epoch - 60UL) : 0U;
}

void alarm_service_tick(uint32_t now_ms)
{
    ModelDomainState domain_state;
    uint32_t epoch;
    uint32_t window_start;
    (void)now_ms;

    if (model_get_domain_state(&domain_state) == NULL || !domain_state.time_valid) {
        g_last_eval_epoch = 0U;
        return;
    }

    epoch = model_datetime_to_epoch(&domain_state.now);

    if (g_last_eval_epoch == 0U) {
        g_last_eval_epoch = epoch;
        return;
    }

    if (epoch < g_last_eval_epoch) {
        g_last_eval_epoch = epoch;
    }

    if (epoch - g_last_eval_epoch > ALARM_EVAL_LOOKBACK_MAX_S) {
        window_start = epoch - ALARM_EVAL_LOOKBACK_MAX_S;
    } else {
        window_start = g_last_eval_epoch;
    }

    if (domain_state.alarm_ringing_index < APP_MAX_ALARMS) {
        g_last_eval_epoch = epoch;
        return;
    }

    for (uint8_t i = 0; i < APP_MAX_ALARMS; ++i) {
        const AlarmState *a = &domain_state.alarms[i];
        bool due = false;
        uint16_t due_day_id = 0U;

        if (!a->enabled || a->ringing) {
            continue;
        }

        if (a->snooze_until_epoch != 0U && a->snooze_until_epoch > window_start && a->snooze_until_epoch <= epoch) {
            due = true;
            due_day_id = (uint16_t)(a->snooze_until_epoch / 86400UL);
        } else if (alarm_due_in_window(a, window_start, epoch, &due_day_id)) {
            due = true;
        }

        if (due) {
            model_trigger_alarm(i, due_day_id);
            break;
        }
    }

    g_last_eval_epoch = epoch;
}


bool alarm_service_is_initialized(void)
{
    return g_alarm_service_initialized;
}

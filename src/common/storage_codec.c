#include "common/storage_codec.h"
#include "app_config.h"

uint16_t storage_pack_settings0(const SettingsState *s)
{
    uint16_t v = 0;
    v |= (uint16_t)(s->brightness & 0x03U);
    v |= (uint16_t)((s->watchface & 0x03U) << 2);
    v |= (uint16_t)((s->vibrate ? 1U : 0U) << 4);
    v |= (uint16_t)((s->auto_wake ? 1U : 0U) << 5);
    v |= (uint16_t)((s->dnd ? 1U : 0U) << 6);
    v |= (uint16_t)((s->show_seconds ? 1U : 0U) << 7);
    v |= (uint16_t)((s->screen_timeout_idx & 0x03U) << 8);
    v |= (uint16_t)((s->auto_sleep ? 1U : 0U) << 10);
    v |= (uint16_t)((s->animations ? 1U : 0U) << 11);
    v |= (uint16_t)((s->sensor_sensitivity & 0x03U) << 12);
    return v;
}

uint16_t storage_pack_settings1(const SettingsState *s)
{
    return (uint16_t)APP_CLAMP(s->goal, 1000U, 30000U);
}

uint16_t storage_pack_settings2(const SettingsState *s)
{
    return 0xA500U | (uint16_t)(APP_CLAMP(s->goal / 500U, 0U, 60U) & 0x003FU);
}

void storage_unpack_settings0(uint16_t v, SettingsState *s)
{
    s->brightness = (uint8_t)(v & 0x03U);
    s->watchface = (uint8_t)((v >> 2) & 0x03U);
    s->vibrate = ((v >> 4) & 0x01U) != 0U;
    s->auto_wake = ((v >> 5) & 0x01U) != 0U;
    s->dnd = ((v >> 6) & 0x01U) != 0U;
    s->show_seconds = ((v >> 7) & 0x01U) != 0U;
    s->screen_timeout_idx = (uint8_t)((v >> 8) & 0x03U);
    s->auto_sleep = ((v >> 10) & 0x01U) != 0U;
    s->animations = ((v >> 11) & 0x01U) != 0U;
    s->sensor_sensitivity = (uint8_t)((v >> 12) & 0x03U);
}

uint16_t storage_pack_alarm_word(const AlarmState *alarm)
{
    uint16_t v = 0;
    uint8_t mask = alarm->repeat_mask;
    v |= (uint16_t)(alarm->hour & 0x1FU);
    v |= (uint16_t)((alarm->minute & 0x3FU) << 5);
    v |= (uint16_t)((alarm->enabled ? 1U : 0U) << 11);
    v |= (uint16_t)((mask & 0x07U) << 12);
    return v;
}

uint16_t storage_pack_alarm_meta(const AlarmState *a0, const AlarmState *a1, const AlarmState *a2)
{
    uint16_t v = 0;
    v |= (uint16_t)(((a0->repeat_mask >> 3) & 0x0FU));
    v |= (uint16_t)(((a1->repeat_mask >> 3) & 0x0FU) << 4);
    v |= (uint16_t)(((a2->repeat_mask >> 3) & 0x0FU) << 8);
    return v;
}

void storage_unpack_alarm_word(uint16_t v, uint8_t high_bits, AlarmState *alarm, uint8_t index)
{
    alarm->hour = (uint8_t)(v & 0x1FU);
    alarm->minute = (uint8_t)((v >> 5) & 0x3FU);
    alarm->enabled = ((v >> 11) & 0x01U) != 0U;
    alarm->repeat_mask = (uint8_t)(((v >> 12) & 0x07U) | ((high_bits & 0x0FU) << 3));
    alarm->label_id = index;
    alarm->ringing = false;
    alarm->snooze_until_epoch = 0U;
    alarm->last_trigger_day = 0U;
}

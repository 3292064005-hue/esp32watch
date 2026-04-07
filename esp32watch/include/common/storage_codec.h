#ifndef COMMON_STORAGE_CODEC_H
#define COMMON_STORAGE_CODEC_H

#include <stdint.h>
#include "model.h"

uint16_t storage_pack_settings0(const SettingsState *s);
uint16_t storage_pack_settings1(const SettingsState *s);
uint16_t storage_pack_settings2(const SettingsState *s);
void storage_unpack_settings0(uint16_t v, SettingsState *s);
uint16_t storage_pack_alarm_word(const AlarmState *alarm);
uint16_t storage_pack_alarm_meta(const AlarmState *a0, const AlarmState *a1, const AlarmState *a2);
void storage_unpack_alarm_word(uint16_t v, uint8_t high_bits, AlarmState *alarm, uint8_t index);

#endif

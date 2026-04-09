#ifndef DRV_BUZZER_H
#define DRV_BUZZER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void drv_buzzer_init(void);
void drv_buzzer_tick(uint32_t now_ms);
void drv_buzzer_set_playback_mode(bool enabled);
void drv_buzzer_play_tone(uint16_t hz);
void drv_buzzer_stop(void);
bool drv_buzzer_is_available(void);
bool drv_buzzer_is_active(void);

#ifdef __cplusplus
}
#endif

#endif

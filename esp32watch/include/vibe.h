#ifndef VIBE_H
#define VIBE_H

#include <stdbool.h>
#include <stdint.h>
#include "model.h"

void vibe_init(void);
void vibe_tick(uint32_t now_ms, PopupType popup, bool enabled);
void vibe_pulse(uint16_t duration_ms);

#endif

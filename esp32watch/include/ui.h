#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

void ui_init(void);
void ui_tick(uint32_t now_ms);
bool ui_should_render(uint32_t now_ms);
void ui_render(void);
void ui_request_render(void);
void ui_wake(void);
bool ui_is_sleeping(void);

bool ui_is_initialized(void);

#endif

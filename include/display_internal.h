#ifndef DISPLAY_INTERNAL_H
#define DISPLAY_INTERNAL_H

#include "display.h"
#include "app_config.h"
#include "platform_api.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t g_oled_buffer[OLED_BUFFER_SIZE];
extern uint8_t g_prev_buffer[OLED_BUFFER_SIZE];

bool display_backend_init(void);
void display_backend_set_contrast(uint8_t value);
void display_backend_set_sleep(bool sleep);
bool display_backend_flush(const uint8_t *buffer);

void display_core_note_full_refresh(void);
void display_core_note_tx_error(PlatformStatus status);

PlatformStatus display_core_write_cmds(const uint8_t *cmds, uint8_t count);
PlatformStatus display_core_write_data_chunk(const uint8_t *data, uint8_t len);
void display_core_set_window(uint8_t page, uint8_t col_start, uint8_t col_end);

#ifdef __cplusplus
}
#endif

#endif


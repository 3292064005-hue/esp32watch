#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void display_init(void);
void display_set_contrast(uint8_t value);
void display_sleep(bool sleep);
void display_clear(void);
bool display_present(void);
void display_invalidate_cache(void);
uint32_t display_get_present_count(void);
uint32_t display_get_full_refresh_count(void);
uint32_t display_get_tx_error_count(void);

void display_draw_pixel(int16_t x, int16_t y, bool color);
void display_draw_hline(int16_t x, int16_t y, int16_t w, bool color);
void display_draw_vline(int16_t x, int16_t y, int16_t h, bool color);
void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);
void display_draw_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);
void display_fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color);
void display_draw_circle(int16_t x0, int16_t y0, int16_t r, bool color);
void display_fill_circle(int16_t x0, int16_t y0, int16_t r, bool color);

void display_draw_char_5x7(int16_t x, int16_t y, char c, bool color);
void display_draw_text_5x7(int16_t x, int16_t y, const char *text, bool color);
void display_draw_text_centered_5x7(int16_t x, int16_t y, int16_t w, const char *text, bool color);

void display_draw_big_digit(int16_t x, int16_t y, uint8_t digit, uint8_t scale, bool color);
void display_draw_big_colon(int16_t x, int16_t y, uint8_t scale, bool color, bool on);

void display_draw_battery_icon(int16_t x, int16_t y, uint8_t percent, bool charging, bool invert);
void display_draw_step_icon(int16_t x, int16_t y, bool invert);
void display_draw_alarm_icon(int16_t x, int16_t y, bool invert);
void display_draw_timer_icon(int16_t x, int16_t y, bool invert);
void display_draw_gear_icon(int16_t x, int16_t y, bool invert);
void display_draw_info_icon(int16_t x, int16_t y, bool invert);
void display_draw_progress_bar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, bool invert);

void display_draw_wifi_icon(int16_t x, int16_t y, bool connected, bool invert);

#ifdef __cplusplus
}
#endif

#endif

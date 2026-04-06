#include "display_internal.h"

void display_draw_pixel(int16_t x, int16_t y, bool color)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }
    uint8_t page = (uint8_t)(y / 8);
    uint16_t idx = (uint16_t)x + (uint16_t)page * OLED_WIDTH;
    uint8_t mask = (uint8_t)(1U << (y & 7));
    if (color) {
        g_oled_buffer[idx] |= mask;
    } else {
        g_oled_buffer[idx] &= (uint8_t)~mask;
    }
}

void display_draw_hline(int16_t x, int16_t y, int16_t w, bool color)
{
    for (int16_t i = 0; i < w; ++i) {
        display_draw_pixel(x + i, y, color);
    }
}

void display_draw_vline(int16_t x, int16_t y, int16_t h, bool color)
{
    for (int16_t i = 0; i < h; ++i) {
        display_draw_pixel(x, y + i, color);
    }
}

void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    if (w <= 0 || h <= 0) return;
    display_draw_hline(x, y, w, color);
    display_draw_hline(x, y + h - 1, w, color);
    display_draw_vline(x, y, h, color);
    display_draw_vline(x + w - 1, y, h, color);
}

void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    for (int16_t yy = 0; yy < h; ++yy) {
        display_draw_hline(x, y + yy, w, color);
    }
}

void display_draw_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    display_draw_hline(x + 2, y, w - 4, color);
    display_draw_hline(x + 2, y + h - 1, w - 4, color);
    display_draw_vline(x, y + 2, h - 4, color);
    display_draw_vline(x + w - 1, y + 2, h - 4, color);
    display_draw_pixel(x + 1, y + 1, color);
    display_draw_pixel(x + w - 2, y + 1, color);
    display_draw_pixel(x + 1, y + h - 2, color);
    display_draw_pixel(x + w - 2, y + h - 2, color);
}

void display_fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool color)
{
    display_fill_rect(x + 1, y, w - 2, h, color);
    display_fill_rect(x, y + 1, w, h - 2, color);
}

void display_draw_circle(int16_t x0, int16_t y0, int16_t r, bool color)
{
    int16_t x = r, y = 0;
    int16_t err = 0;
    while (x >= y) {
        display_draw_pixel(x0 + x, y0 + y, color);
        display_draw_pixel(x0 + y, y0 + x, color);
        display_draw_pixel(x0 - y, y0 + x, color);
        display_draw_pixel(x0 - x, y0 + y, color);
        display_draw_pixel(x0 - x, y0 - y, color);
        display_draw_pixel(x0 - y, y0 - x, color);
        display_draw_pixel(x0 + y, y0 - x, color);
        display_draw_pixel(x0 + x, y0 - y, color);
        y++;
        if (err <= 0) err += 2 * y + 1;
        if (err > 0) { x--; err -= 2 * x + 1; }
    }
}

void display_fill_circle(int16_t x0, int16_t y0, int16_t r, bool color)
{
    for (int16_t y = -r; y <= r; ++y) {
        for (int16_t x = -r; x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                display_draw_pixel(x0 + x, y0 + y, color);
            }
        }
    }
}


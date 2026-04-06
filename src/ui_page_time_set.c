#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include "model.h"
#include <stdio.h>

static uint8_t days_in_month_local(uint16_t year, uint8_t month)
{
    static const uint8_t dim_tbl[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t dim = dim_tbl[(month - 1U) % 12U];
    bool leap = ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
    if (month == 2U && leap) dim = 29U;
    return dim;
}

static void clamp_edit_time(DateTime *edit_time)
{
    if (edit_time == NULL) {
        return;
    }
    if (edit_time->month < 1U) edit_time->month = 1U;
    if (edit_time->month > 12U) edit_time->month = 12U;
    if (edit_time->day < 1U) edit_time->day = 1U;
    {
        uint8_t dim = days_in_month_local(edit_time->year, edit_time->month);
        if (edit_time->day > dim) edit_time->day = dim;
    }
    if (edit_time->hour > 23U) edit_time->hour = 23U;
    if (edit_time->minute > 59U) edit_time->minute = 59U;
    edit_time->second = 0U;
}

static void adjust_time_field(int8_t delta)
{
    DateTime edit_time = ui_runtime_get_edit_time();

    if (ui_runtime_get_time_field() == 0U) edit_time.year = (uint16_t)APP_CLAMP((int)edit_time.year + delta, 2024, 2099);
    else if (ui_runtime_get_time_field() == 1U) {
        int m = (int)edit_time.month + delta;
        if (m < 1) m = 12;
        if (m > 12) m = 1;
        edit_time.month = (uint8_t)m;
    } else if (ui_runtime_get_time_field() == 2U) {
        int d = (int)edit_time.day + delta;
        int dim = days_in_month_local(edit_time.year, edit_time.month);
        if (d < 1) d = dim;
        if (d > dim) d = 1;
        edit_time.day = (uint8_t)d;
    } else if (ui_runtime_get_time_field() == 3U) {
        int h = (int)edit_time.hour + delta;
        if (h < 0) h = 23;
        if (h > 23) h = 0;
        edit_time.hour = (uint8_t)h;
    } else if (ui_runtime_get_time_field() == 4U) {
        int mm = (int)edit_time.minute + delta;
        if (mm < 0) mm = 59;
        if (mm > 59) mm = 0;
        edit_time.minute = (uint8_t)mm;
    }
    clamp_edit_time(&edit_time);
    ui_runtime_set_edit_time(&edit_time);
}

void ui_page_time_set_render(PageId page, int16_t ox)
{
    (void)page;
    char line[24];
    ui_core_draw_header(ox, "Time Set");
    { DateTime edit_time = ui_runtime_get_edit_time();
    snprintf(line, sizeof(line), "%04u-%02u-%02u", edit_time.year, edit_time.month, edit_time.day);
    display_draw_text_centered_5x7(ox, 20, 128, line, true);
    snprintf(line, sizeof(line), "%02u:%02u", edit_time.hour, edit_time.minute);
    }
    display_draw_text_centered_5x7(ox, 32, 128, line, true);
    {
        const char *fields[] = {"Y", "M", "D", "H", "Min", "OK"};
        for (uint8_t i = 0; i < 6; ++i) {
            int16_t bx = ox + 6 + i * 20;
            bool sel = i == ui_runtime_get_time_field();
            if (sel) display_fill_round_rect(bx, 49, 18, 10, true);
            display_draw_text_centered_5x7(bx, 51, 18, fields[i], !sel);
        }
    }
}

bool ui_page_time_set_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) {
        if (e->id == KEY_ID_UP) adjust_time_field(1);
        else if (e->id == KEY_ID_DOWN) adjust_time_field(-1);
        else if (e->id == KEY_ID_OK) {
            if (ui_runtime_get_time_field() == 5U) {
                DateTime edit_time = ui_runtime_get_edit_time();
                ui_request_set_datetime(&edit_time);
                ui_core_go(PAGE_SETTINGS, -1, now_ms);
            } else {
                ui_runtime_set_time_field((uint8_t)(ui_runtime_get_time_field() + 1U));
            }
        } else if (e->id == KEY_ID_BACK) {
            ui_core_go(PAGE_SETTINGS, -1, now_ms);
        } else return false;
        return true;
    }
    return false;
}

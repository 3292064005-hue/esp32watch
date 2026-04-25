#include "ui_page_games_support.h"

#include <stdio.h>

#include "display.h"
#include "platform_api.h"
#include "ui_internal.h"

int16_t game_clamp_i16(int16_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

uint16_t game_clamp_u16(uint16_t value, uint16_t min_value, uint16_t max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

bool game_rect_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                       int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return ax < (bx + bw) && (ax + aw) > bx && ay < (by + bh) && (ay + ah) > by;
}

uint16_t game_rand_next(uint16_t *seed)
{
    if (seed == NULL) return 0U;
    *seed = (uint16_t)((*seed * 109U) + 89U);
    return *seed;
}

uint32_t game_now_ms(void)
{
    return platform_time_now_ms();
}

void game_draw_arena(int16_t ox, const char *title, const char *footer)
{
    ui_core_draw_header(ox, title);
    display_draw_round_rect(ox + GAME_ARENA_X, GAME_ARENA_Y, GAME_ARENA_W, GAME_ARENA_H, true);
    ui_core_draw_footer_hint(ox, footer);
}

bool game_should_tick(uint32_t *last_ms, uint32_t interval_ms)
{
    uint32_t now_ms = game_now_ms();

    if (last_ms == NULL) return false;
    if (ui_runtime_is_transition_render()) {
        *last_ms = now_ms;
        return false;
    }
    if ((now_ms - *last_ms) < interval_ms) return false;
    *last_ms = now_ms;
    return true;
}

bool game_ready_elapsed(uint32_t ready_until_ms)
{
    return ready_until_ms != 0U && game_now_ms() >= ready_until_ms;
}

void game_draw_center_notice(int16_t ox, int16_t width, const char *line_a, const char *line_b)
{
    int16_t x = ox + ((128 - width) / 2);

    display_fill_round_rect(x, 27, width, line_b == NULL ? 12 : 18, false);
    display_draw_round_rect(x, 27, width, line_b == NULL ? 12 : 18, true);
    display_draw_text_centered_5x7(x, 30, width, line_a, true);
    if (line_b != NULL) {
        display_draw_text_centered_5x7(x, 38, width, line_b, true);
    }
}

void game_draw_result_overlay(int16_t ox, const char *title, uint16_t score, bool new_high, const char *detail)
{
    char line[18];
    int16_t box_h = detail == NULL ? 20 : 28;

    display_fill_round_rect(ox + 14, 20, 100, box_h, false);
    display_draw_round_rect(ox + 14, 20, 100, box_h, true);
    display_draw_text_centered_5x7(ox + 14, 23, 100, title, true);
    snprintf(line, sizeof(line), "SCORE %u", (unsigned)score);
    display_draw_text_5x7(ox + 22, 32, line, true);
    display_draw_text_right_5x7(ox + 104, 32, new_high ? "NEW HI" : "BK", true);
    if (detail != NULL) {
        display_draw_text_centered_5x7(ox + 18, 40, 92, detail, true);
    }
}

void game_report_result(GameId game_id, uint16_t score, bool *new_high, bool *reported)
{
    if (reported == NULL || new_high == NULL || *reported) {
        return;
    }

    *new_high = score > model_get_game_high_score(game_id);
    if (*new_high) {
        ui_request_set_game_high_score(game_id, score);
    }
    *reported = true;
}

void game_go_detail(uint32_t now_ms)
{
    ui_core_go(PAGE_GAME_DETAIL, -1, now_ms);
}

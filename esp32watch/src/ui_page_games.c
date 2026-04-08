#include "ui_internal.h"
#include "display.h"
#include "platform_api.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define GAME_ARENA_X 8
#define GAME_ARENA_Y 14
#define GAME_ARENA_W 112
#define GAME_ARENA_H 36
#define SNAKE_CELL_SIZE 4
#define SNAKE_COLS 27
#define SNAKE_ROWS 8
#define SNAKE_MAX_CELLS (SNAKE_COLS * SNAKE_ROWS)
#define TETRIS_COLS 10
#define TETRIS_ROWS 16
#define TETRIS_CELL_SIZE 2
#define SHOOTER_MAX_BULLETS 4
#define SHOOTER_MAX_ENEMIES 5
#define SHOOTER_MAX_STARS 6
#define BREAKOUT_MAX_LEVELS 3
#define GAME_READY_DELAY_MS 800U
#define GAME_RESULT_HOLD_MS 300U
#define DINO_EARLY_MS 8000U
#define DINO_MID_MS 20000U

typedef enum { GAME_MODE_READY = 0, GAME_MODE_PLAYING, GAME_MODE_GAME_OVER, GAME_MODE_WIN } GameMode;
typedef enum { DIR_UP = 0, DIR_RIGHT, DIR_DOWN, DIR_LEFT } SnakeDirection;

typedef struct {
    bool initialized;
    GameMode mode;
    uint32_t last_ms;
    uint32_t ready_until_ms;
    uint16_t score;
    uint16_t final_score;
    uint8_t lives;
    uint8_t level;
    bool life_lost;
    bool auto_launch_ready;
    bool new_high;
    bool result_reported;
    int16_t paddle_x;
    int16_t ball_x;
    int16_t ball_y;
    int8_t ball_vx;
    int8_t ball_vy;
    uint8_t bricks[3][6];
    uint8_t bricks_left;
} BreakoutState;

typedef struct {
    bool initialized;
    GameMode mode;
    uint32_t last_ms;
    uint32_t elapsed_ms;
    uint16_t score;
    uint16_t final_score;
    uint16_t seed;
    int16_t runner_y;
    int16_t velocity_y;
    int16_t obstacle_x;
    int16_t obstacle_y;
    int16_t obstacle_w;
    int16_t obstacle_h;
    uint8_t speed;
    uint8_t obstacle_type;
    bool new_high;
    bool result_reported;
} DinoState;

typedef struct {
    bool initialized;
    GameMode mode;
    uint32_t last_ms;
    uint32_t round_started_ms;
    uint16_t score;
    uint16_t final_score;
    uint8_t player_score;
    uint8_t ai_score;
    uint8_t ai_speed;
    uint8_t ai_reaction_frames;
    uint8_t ai_reaction_tick;
    bool serve_to_right;
    bool new_high;
    bool result_reported;
    int16_t player_y;
    int16_t ai_y;
    int16_t ball_x;
    int16_t ball_y;
    int8_t ball_vx;
    int8_t ball_vy;
} PongState;

typedef struct {
    bool initialized;
    GameMode mode;
    bool paused;
    uint32_t last_step_ms;
    uint16_t score;
    uint16_t final_score;
    uint16_t seed;
    uint16_t length;
    SnakeDirection dir;
    int8_t queued_turn;
    uint8_t food_x;
    uint8_t food_y;
    uint8_t food_type;
    uint32_t bonus_until_ms;
    bool new_high;
    bool result_reported;
    uint8_t body_x[SNAKE_MAX_CELLS];
    uint8_t body_y[SNAKE_MAX_CELLS];
} SnakeState;

typedef struct {
    bool initialized;
    GameMode mode;
    bool paused;
    uint32_t last_step_ms;
    uint16_t score;
    uint16_t final_score;
    uint16_t lines;
    uint16_t seed;
    uint8_t level;
    uint8_t bag[7];
    uint8_t bag_index;
    bool new_high;
    bool result_reported;
    uint8_t board[TETRIS_ROWS][TETRIS_COLS];
    uint8_t piece;
    uint8_t next_piece;
    uint8_t rotation;
    int8_t piece_x;
    int8_t piece_y;
} TetrisState;

typedef struct { bool active; int16_t x; int16_t y; } ShooterBullet;
typedef struct { bool active; int16_t x; int16_t y; int8_t speed; int8_t phase; uint8_t type; } ShooterEnemy;
typedef struct { int16_t x; int16_t y; } ShooterStar;

typedef struct {
    bool initialized;
    GameMode mode;
    uint32_t last_step_ms;
    uint32_t last_spawn_ms;
    uint32_t last_fire_ms;
    uint32_t invulnerable_until_ms;
    uint16_t score;
    uint16_t final_score;
    uint16_t seed;
    uint8_t faults;
    uint32_t wave_relief_until_ms;
    bool new_high;
    bool result_reported;
    int16_t player_y;
    ShooterBullet bullets[SHOOTER_MAX_BULLETS];
    ShooterEnemy enemies[SHOOTER_MAX_ENEMIES];
    ShooterStar stars[SHOOTER_MAX_STARS];
} ShooterState;

static BreakoutState g_breakout;
static DinoState g_dino;
static PongState g_pong;
static SnakeState g_snake;
static TetrisState g_tetris;
static ShooterState g_shooter;

static int16_t game_clamp_i16(int16_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static uint16_t game_clamp_u16(uint16_t value, uint16_t min_value, uint16_t max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool game_rect_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                              int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return ax < (bx + bw) && (ax + aw) > bx && ay < (by + bh) && (ay + ah) > by;
}

static uint16_t game_rand_next(uint16_t *seed)
{
    if (seed == NULL) return 0U;
    *seed = (uint16_t)((*seed * 109U) + 89U);
    return *seed;
}

static uint32_t game_now_ms(void)
{
    return platform_time_now_ms();
}

static void game_draw_arena(int16_t ox, const char *title, const char *footer)
{
    ui_core_draw_header(ox, title);
    display_draw_round_rect(ox + GAME_ARENA_X, GAME_ARENA_Y, GAME_ARENA_W, GAME_ARENA_H, true);
    ui_core_draw_footer_hint(ox, footer);
}

static bool game_should_tick(uint32_t *last_ms, uint32_t interval_ms)
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

static bool game_ready_elapsed(uint32_t ready_until_ms)
{
    return ready_until_ms != 0U && game_now_ms() >= ready_until_ms;
}

static void game_draw_center_notice(int16_t ox, int16_t width, const char *line_a, const char *line_b)
{
    int16_t x = ox + ((128 - width) / 2);

    display_fill_round_rect(x, 27, width, line_b == NULL ? 12 : 18, false);
    display_draw_round_rect(x, 27, width, line_b == NULL ? 12 : 18, true);
    display_draw_text_centered_5x7(x, 30, width, line_a, true);
    if (line_b != NULL) {
        display_draw_text_centered_5x7(x, 38, width, line_b, true);
    }
}

static void game_draw_result_overlay(int16_t ox, const char *title, uint16_t score, bool new_high, const char *detail)
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

static void game_report_result(GameId game_id, uint16_t score, bool *new_high, bool *reported)
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

static void game_go_detail(uint32_t now_ms)
{
    ui_core_go(PAGE_GAME_DETAIL, -1, now_ms);
}

static void breakout_reset_round(void)
{
    uint32_t now_ms = game_now_ms();

    g_breakout.paddle_x = GAME_ARENA_X + (GAME_ARENA_W / 2) - 9;
    g_breakout.ball_x = g_breakout.paddle_x + 9;
    g_breakout.ball_y = GAME_ARENA_Y + GAME_ARENA_H - 10;
    g_breakout.ball_vx = (int8_t)(g_breakout.level > 1U ? 2 : 1);
    g_breakout.ball_vy = (int8_t)(-1 - (g_breakout.level > 2U ? 1 : 0));
    g_breakout.ready_until_ms = now_ms + GAME_READY_DELAY_MS;
    g_breakout.last_ms = now_ms;
}

static void breakout_seed_level(uint8_t level)
{
    for (uint8_t row = 0U; row < 3U; ++row) {
        for (uint8_t col = 0U; col < 6U; ++col) {
            uint8_t brick = 1U;

            if ((level >= 2U) && ((row + col) % 3U == 0U)) {
                brick = 2U;
            }
            if ((level >= 3U) && ((row + col) % 4U == 1U)) {
                brick = 3U;
            }
            g_breakout.bricks[row][col] = brick;
        }
    }
    g_breakout.bricks_left = 18U;
}

static void breakout_reset_state(void)
{
    memset(&g_breakout, 0, sizeof(g_breakout));
    g_breakout.initialized = true;
    g_breakout.mode = GAME_MODE_READY;
    g_breakout.last_ms = game_now_ms();
    g_breakout.lives = 3U;
    g_breakout.level = 1U;
    breakout_seed_level(g_breakout.level);
    breakout_reset_round();
}

static void breakout_launch_ball(void)
{
    g_breakout.mode = GAME_MODE_PLAYING;
    g_breakout.life_lost = false;
    g_breakout.auto_launch_ready = false;
    g_breakout.ball_x = g_breakout.paddle_x + 9;
    g_breakout.ball_y = GAME_ARENA_Y + GAME_ARENA_H - 10;
    g_breakout.ball_vx = (g_breakout.score & 0x01U) == 0U ? (int8_t)(g_breakout.level > 1U ? 2 : 1) : (int8_t)-(g_breakout.level > 1U ? 2 : 1);
    g_breakout.ball_vy = (int8_t)(-1 - (g_breakout.level > 2U ? 1 : 0));
    g_breakout.last_ms = game_now_ms();
}

static void breakout_move_paddle(int8_t dir)
{
    g_breakout.paddle_x += (int16_t)(dir * 4);
    g_breakout.paddle_x = game_clamp_i16(g_breakout.paddle_x, GAME_ARENA_X + 2, GAME_ARENA_X + GAME_ARENA_W - 20);
    if (g_breakout.mode != GAME_MODE_PLAYING) g_breakout.ball_x = g_breakout.paddle_x + 9;
}

static void breakout_handle_miss(void)
{
    if (g_breakout.lives > 1U) {
        g_breakout.lives--;
        g_breakout.life_lost = true;
        g_breakout.auto_launch_ready = true;
        g_breakout.mode = GAME_MODE_READY;
        breakout_reset_round();
    } else {
        g_breakout.lives = 0U;
        g_breakout.final_score = g_breakout.score;
        g_breakout.mode = GAME_MODE_GAME_OVER;
    }
}

static void breakout_update(void)
{
    static const int16_t brick_x[6] = {12, 29, 46, 63, 80, 97};
    static const int16_t brick_y[3] = {18, 25, 32};
    uint8_t row;
    uint8_t col;
    int16_t paddle_y = GAME_ARENA_Y + GAME_ARENA_H - 5;

    if (g_breakout.mode == GAME_MODE_READY && g_breakout.auto_launch_ready && game_ready_elapsed(g_breakout.ready_until_ms)) {
        breakout_launch_ball();
    }
    if (g_breakout.mode != GAME_MODE_PLAYING || !game_should_tick(&g_breakout.last_ms, 50U)) return;

    g_breakout.ball_x += g_breakout.ball_vx;
    g_breakout.ball_y += g_breakout.ball_vy;
    if (g_breakout.ball_x <= (GAME_ARENA_X + 2) || g_breakout.ball_x >= (GAME_ARENA_X + GAME_ARENA_W - 3)) {
        g_breakout.ball_vx = (int8_t)(-g_breakout.ball_vx);
        g_breakout.ball_x += g_breakout.ball_vx;
    }
    if (g_breakout.ball_y <= (GAME_ARENA_Y + 2)) {
        g_breakout.ball_vy = 1;
        g_breakout.ball_y = GAME_ARENA_Y + 3;
    }
    if (g_breakout.ball_vy > 0 &&
        game_rect_overlap(g_breakout.ball_x - 1, g_breakout.ball_y - 1, 3, 3, g_breakout.paddle_x, paddle_y, 18, 3)) {
        int16_t offset = g_breakout.ball_x - (g_breakout.paddle_x + 9);

        g_breakout.ball_vy = (int8_t)(-1 - (g_breakout.level > 2U ? 1 : 0));
        if (offset <= -7) g_breakout.ball_vx = -2;
        else if (offset <= -3) g_breakout.ball_vx = -1;
        else if (offset < 3) g_breakout.ball_vx = 0;
        else if (offset < 7) g_breakout.ball_vx = 1;
        else g_breakout.ball_vx = 2;
        g_breakout.ball_y = paddle_y - 2;
    }
    for (row = 0U; row < 3U; ++row) {
        for (col = 0U; col < 6U; ++col) {
            if (g_breakout.bricks[row][col] == 0U) continue;
            if (!game_rect_overlap(g_breakout.ball_x - 1, g_breakout.ball_y - 1, 3, 3, brick_x[col], brick_y[row], 15, 5)) continue;
            if (g_breakout.bricks[row][col] == 2U) {
                g_breakout.bricks[row][col] = 1U;
                g_breakout.score = (uint16_t)(g_breakout.score + 5U);
            } else {
                uint16_t add = (uint16_t)(30U - row * 10U);

                if (g_breakout.bricks[row][col] == 3U) {
                    add = (uint16_t)(add + 25U);
                }
                g_breakout.bricks[row][col] = 0U;
                if (g_breakout.bricks_left > 0U) g_breakout.bricks_left--;
                g_breakout.score = (uint16_t)(g_breakout.score + add);
            }
            g_breakout.ball_vy = (int8_t)(-g_breakout.ball_vy);
            if (g_breakout.bricks_left == 0U) {
                if (g_breakout.level < BREAKOUT_MAX_LEVELS) {
                    g_breakout.level++;
                    g_breakout.score = (uint16_t)(g_breakout.score + 80U);
                    g_breakout.mode = GAME_MODE_READY;
                    breakout_seed_level(g_breakout.level);
                    breakout_reset_round();
                } else {
                    g_breakout.final_score = (uint16_t)(g_breakout.score + (g_breakout.lives * 40U) + 120U);
                    g_breakout.mode = GAME_MODE_WIN;
                }
            }
            return;
        }
    }
    if (g_breakout.ball_y >= (GAME_ARENA_Y + GAME_ARENA_H - 1)) breakout_handle_miss();
}

static void breakout_render_internal(int16_t ox)
{
    char status[20];
    const char *footer;
    uint8_t row;
    uint8_t col;

    if (!g_breakout.initialized) breakout_reset_state();
    breakout_update();
    footer = (g_breakout.mode == GAME_MODE_PLAYING) ? "UP Left  DN Right" : "OK Start  BK Detail";
    if (g_breakout.mode == GAME_MODE_GAME_OVER || g_breakout.mode == GAME_MODE_WIN) footer = "OK Retry  BK Detail";
    game_draw_arena(ox, "Breakout", footer);
    snprintf(status, sizeof(status), "S %u L %u LV%u", (unsigned)g_breakout.score, (unsigned)g_breakout.lives, (unsigned)g_breakout.level);
    display_draw_text_right_5x7(ox + 118, 2, status, true);
    for (row = 0U; row < 3U; ++row) {
        for (col = 0U; col < 6U; ++col) {
            uint8_t brick = g_breakout.bricks[row][col];

            if (brick == 0U) continue;
            if (brick == 1U) {
                display_fill_round_rect(ox + 12 + col * 17, 18 + row * 7, 15, 5, true);
            } else if (brick == 2U) {
                display_draw_round_rect(ox + 12 + col * 17, 18 + row * 7, 15, 5, true);
                display_draw_hline(ox + 15 + col * 17, 20 + row * 7, 9, true);
            } else {
                display_fill_round_rect(ox + 12 + col * 17, 18 + row * 7, 15, 5, true);
                display_draw_pixel(ox + 19 + col * 17, 20 + row * 7, false);
            }
        }
    }
    display_fill_round_rect(ox + g_breakout.paddle_x, GAME_ARENA_Y + GAME_ARENA_H - 5, 18, 3, true);
    display_fill_circle(ox + g_breakout.ball_x, g_breakout.ball_y, 1, true);
    if (g_breakout.mode == GAME_MODE_READY) {
        game_draw_center_notice(ox, 82, g_breakout.life_lost ? "LIFE LOST" : "PRESS OK",
                                g_breakout.auto_launch_ready ? "AUTO SERVE" : NULL);
    } else if (g_breakout.mode == GAME_MODE_GAME_OVER) {
        game_report_result(GAME_ID_BREAKOUT, g_breakout.final_score, &g_breakout.new_high, &g_breakout.result_reported);
        game_draw_result_overlay(ox, "GAME OVER", g_breakout.final_score, g_breakout.new_high, NULL);
    } else if (g_breakout.mode == GAME_MODE_WIN) {
        game_report_result(GAME_ID_BREAKOUT, g_breakout.final_score, &g_breakout.new_high, &g_breakout.result_reported);
        game_draw_result_overlay(ox, "YOU WIN", g_breakout.final_score, g_breakout.new_high, NULL);
    }
}

static bool breakout_handle_internal(const KeyEvent *e, uint32_t now_ms)
{
    if (e == NULL) return false;
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_UP) {
        breakout_move_paddle(-1);
        return true;
    }
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_DOWN) {
        breakout_move_paddle(1);
        return true;
    }
    if (e->type != KEY_EVENT_SHORT) return false;
    if (e->id == KEY_ID_BACK) {
        game_go_detail(now_ms);
        return true;
    }
    if (e->id == KEY_ID_OK) {
        if (g_breakout.mode == GAME_MODE_GAME_OVER || g_breakout.mode == GAME_MODE_WIN) breakout_reset_state();
        breakout_launch_ball();
        return true;
    }
    return false;
}

static void dino_spawn_obstacle(void)
{
    uint16_t roll = game_rand_next(&g_dino.seed);
    int16_t ground_y = GAME_ARENA_Y + GAME_ARENA_H - 9;
    uint8_t min_gap = 16U;
    uint8_t extra_gap = 12U;

    if (g_dino.elapsed_ms < DINO_EARLY_MS) {
        g_dino.obstacle_type = 0U;
        min_gap = 24U;
        extra_gap = 14U;
    } else if (g_dino.elapsed_ms < DINO_MID_MS) {
        g_dino.obstacle_type = (uint8_t)(roll % 2U);
        min_gap = 20U;
    } else {
        g_dino.obstacle_type = (uint8_t)(roll % 3U);
    }
    if (g_dino.obstacle_type == 0U) {
        g_dino.obstacle_w = (int16_t)(6 + (roll % 3U));
        g_dino.obstacle_h = 8;
        g_dino.obstacle_y = ground_y;
    } else if (g_dino.obstacle_type == 1U) {
        g_dino.obstacle_w = 9;
        g_dino.obstacle_h = 10;
        g_dino.obstacle_y = ground_y - 2;
    } else {
        g_dino.obstacle_w = 8;
        g_dino.obstacle_h = 5;
        g_dino.obstacle_y = ground_y - 12;
    }
    g_dino.obstacle_x = GAME_ARENA_X + GAME_ARENA_W + (int16_t)min_gap + (int16_t)(roll % extra_gap);
}

static void dino_reset_state(void)
{
    memset(&g_dino, 0, sizeof(g_dino));
    g_dino.initialized = true;
    g_dino.mode = GAME_MODE_READY;
    g_dino.last_ms = game_now_ms();
    g_dino.seed = (uint16_t)g_dino.last_ms;
    g_dino.runner_y = GAME_ARENA_Y + GAME_ARENA_H - 10;
    g_dino.speed = 2U;
    dino_spawn_obstacle();
}

static void dino_jump(void)
{
    int16_t ground_y = GAME_ARENA_Y + GAME_ARENA_H - 10;

    if (g_dino.runner_y >= ground_y) g_dino.velocity_y = -8;
}

static void dino_start(void)
{
    g_dino.mode = GAME_MODE_PLAYING;
    g_dino.last_ms = game_now_ms();
}

static void dino_update(void)
{
    int16_t ground_y = GAME_ARENA_Y + GAME_ARENA_H - 10;
    int16_t runner_x = GAME_ARENA_X + 14;

    if (g_dino.mode != GAME_MODE_PLAYING || !game_should_tick(&g_dino.last_ms, 50U)) return;
    g_dino.elapsed_ms += 50U;
    g_dino.score++;
    if (g_dino.score < 40U) g_dino.speed = 2U;
    else if (g_dino.score < 90U) g_dino.speed = 3U;
    else if (g_dino.score < 150U) g_dino.speed = 4U;
    else g_dino.speed = 5U;
    g_dino.velocity_y += (g_dino.velocity_y < 0) ? 1 : 2;
    if (g_dino.velocity_y > 5) g_dino.velocity_y = 5;
    g_dino.runner_y += g_dino.velocity_y;
    if (g_dino.runner_y > ground_y) {
        g_dino.runner_y = ground_y;
        g_dino.velocity_y = 0;
    }
    g_dino.obstacle_x -= g_dino.speed;
    if ((g_dino.obstacle_x + g_dino.obstacle_w) < GAME_ARENA_X + 1) dino_spawn_obstacle();
    if (game_rect_overlap(runner_x, g_dino.runner_y, 9, 8, g_dino.obstacle_x, g_dino.obstacle_y, g_dino.obstacle_w, g_dino.obstacle_h)) {
        g_dino.final_score = g_dino.score;
        g_dino.mode = GAME_MODE_GAME_OVER;
    }
}

static void dino_render_internal(int16_t ox)
{
    char score_text[18];
    int16_t ground_y = GAME_ARENA_Y + GAME_ARENA_H - 1;
    const char *footer = (g_dino.mode == GAME_MODE_PLAYING) ? "OK Jump  DN Drop" : "OK Start  BK Detail";

    if (!g_dino.initialized) dino_reset_state();
    dino_update();
    game_draw_arena(ox, "Dino", footer);
    snprintf(score_text, sizeof(score_text), "RUN %u SPD %u", (unsigned)g_dino.score, (unsigned)g_dino.speed);
    display_draw_text_right_5x7(ox + 118, 2, score_text, true);
    display_draw_hline(ox + GAME_ARENA_X + 2, ground_y, GAME_ARENA_W - 4, true);
    display_fill_rect(ox + GAME_ARENA_X + 14, g_dino.runner_y, 9, 8, true);
    display_draw_pixel(ox + GAME_ARENA_X + 22, g_dino.runner_y + 2, false);
    display_fill_rect(ox + g_dino.obstacle_x, g_dino.obstacle_y, g_dino.obstacle_w, g_dino.obstacle_h, true);
    if (g_dino.mode == GAME_MODE_READY) {
        game_draw_center_notice(ox, 80, "PRESS OK", "JUMP TO RUN");
    } else if (g_dino.mode == GAME_MODE_GAME_OVER) {
        game_report_result(GAME_ID_DINO, g_dino.final_score, &g_dino.new_high, &g_dino.result_reported);
        game_draw_result_overlay(ox, "RUN OVER", g_dino.final_score, g_dino.new_high, NULL);
    }
}

static bool dino_handle_internal(const KeyEvent *e, uint32_t now_ms)
{
    int16_t ground_y = GAME_ARENA_Y + GAME_ARENA_H - 10;

    if (e == NULL) return false;
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && (e->id == KEY_ID_OK || e->id == KEY_ID_UP)) {
        if (g_dino.mode != GAME_MODE_PLAYING) {
            dino_reset_state();
            dino_start();
        }
        dino_jump();
        return true;
    }
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) &&
        e->id == KEY_ID_DOWN && g_dino.mode == GAME_MODE_PLAYING && g_dino.runner_y < ground_y) {
        g_dino.velocity_y += 4;
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        game_go_detail(now_ms);
        return true;
    }
    return false;
}

static void pong_reset_ball(bool serve_to_right)
{
    g_pong.ball_x = GAME_ARENA_X + (GAME_ARENA_W / 2);
    g_pong.ball_y = GAME_ARENA_Y + (GAME_ARENA_H / 2);
    g_pong.serve_to_right = serve_to_right;
    g_pong.ball_vx = serve_to_right ? 2 : -2;
    g_pong.ball_vy = (g_pong.player_score & 0x01U) == 0U ? 1 : -1;
    g_pong.round_started_ms = game_now_ms();
    g_pong.ai_reaction_tick = 0U;
}

static void pong_reset_state(void)
{
    memset(&g_pong, 0, sizeof(g_pong));
    g_pong.initialized = true;
    g_pong.mode = GAME_MODE_READY;
    g_pong.last_ms = game_now_ms();
    g_pong.player_y = GAME_ARENA_Y + 12;
    g_pong.ai_y = GAME_ARENA_Y + 12;
    g_pong.ai_speed = 1U;
    pong_reset_ball(true);
}

static void pong_move_player(int8_t dir)
{
    g_pong.player_y += (int16_t)(dir * 3);
    g_pong.player_y = game_clamp_i16(g_pong.player_y, GAME_ARENA_Y + 2, GAME_ARENA_Y + GAME_ARENA_H - 12);
}

static void pong_start_round(void)
{
    if (!g_pong.initialized) pong_reset_state();
    if (g_pong.mode == GAME_MODE_GAME_OVER || g_pong.mode == GAME_MODE_WIN) pong_reset_state();
    g_pong.ai_speed = (uint8_t)game_clamp_u16((uint16_t)(1U + (g_pong.ai_score / 2U) + (g_pong.player_score >= 3U ? 1U : 0U)), 1U, 3U);
    g_pong.ai_reaction_frames = (uint8_t)(g_pong.ai_score > g_pong.player_score ? 1U : 2U);
    g_pong.ai_reaction_tick = 0U;
    g_pong.mode = GAME_MODE_PLAYING;
    g_pong.last_ms = game_now_ms();
}

static void pong_score_point(bool player_scored)
{
    if (player_scored) {
        g_pong.player_score++;
        if (g_pong.player_score >= 5U) {
            g_pong.final_score = (uint16_t)(500U + (uint16_t)(5U - g_pong.ai_score) * 20U);
            g_pong.mode = GAME_MODE_WIN;
            return;
        }
        pong_reset_ball(false);
    } else {
        g_pong.ai_score++;
        if (g_pong.ai_score >= 5U) {
            g_pong.final_score = (uint16_t)(g_pong.player_score * 100U);
            g_pong.mode = GAME_MODE_GAME_OVER;
            return;
        }
        pong_reset_ball(true);
    }
    g_pong.mode = GAME_MODE_READY;
}

static void pong_update(void)
{
    if (g_pong.mode != GAME_MODE_PLAYING || !game_should_tick(&g_pong.last_ms, 50U)) return;
    if (g_pong.ai_reaction_tick++ >= g_pong.ai_reaction_frames) {
        int16_t target_y = g_pong.ball_y;
        int16_t dead_zone = (g_pong.ai_score > g_pong.player_score) ? 2 : 4;

        g_pong.ai_reaction_tick = 0U;
        if (target_y < (g_pong.ai_y + 4 - dead_zone)) g_pong.ai_y = (int16_t)(g_pong.ai_y - g_pong.ai_speed);
        else if (target_y > (g_pong.ai_y + 6 + dead_zone)) g_pong.ai_y = (int16_t)(g_pong.ai_y + g_pong.ai_speed);
    }
    g_pong.ai_y = game_clamp_i16(g_pong.ai_y, GAME_ARENA_Y + 2, GAME_ARENA_Y + GAME_ARENA_H - 12);
    g_pong.ball_x += g_pong.ball_vx;
    g_pong.ball_y += g_pong.ball_vy;
    if (g_pong.ball_y <= GAME_ARENA_Y + 2 || g_pong.ball_y >= GAME_ARENA_Y + GAME_ARENA_H - 2) g_pong.ball_vy = (int8_t)(-g_pong.ball_vy);
    if (game_rect_overlap(g_pong.ball_x - 1, g_pong.ball_y - 1, 3, 3, GAME_ARENA_X + 4, g_pong.player_y, 3, 10)) {
        g_pong.ball_vx = 2;
        g_pong.ball_vy = (int8_t)((g_pong.ball_y - (g_pong.player_y + 5)) / 2);
        if ((game_now_ms() - g_pong.round_started_ms) < 350U) {
            g_pong.ball_vy = game_clamp_i16(g_pong.ball_vy, -1, 1);
        }
        if (g_pong.ball_vy == 0) g_pong.ball_vy = 1;
    } else if (game_rect_overlap(g_pong.ball_x - 1, g_pong.ball_y - 1, 3, 3, GAME_ARENA_X + GAME_ARENA_W - 7, g_pong.ai_y, 3, 10)) {
        g_pong.ball_vx = -2;
        g_pong.ball_vy = (int8_t)((g_pong.ball_y - (g_pong.ai_y + 5)) / 2);
        if ((game_now_ms() - g_pong.round_started_ms) < 350U) {
            g_pong.ball_vy = game_clamp_i16(g_pong.ball_vy, -1, 1);
        }
        if (g_pong.ball_vy == 0) g_pong.ball_vy = -1;
    }
    if (g_pong.ball_x < GAME_ARENA_X + 1) pong_score_point(false);
    else if (g_pong.ball_x > GAME_ARENA_X + GAME_ARENA_W - 1) pong_score_point(true);
    g_pong.score = (uint16_t)(g_pong.player_score * 100U + (uint16_t)(5U - g_pong.ai_score) * 10U);
}

static void pong_render_internal(int16_t ox)
{
    char score_text[18];
    const char *footer = (g_pong.mode == GAME_MODE_PLAYING) ? "UP/DN Move  BK Detail" : "OK Serve  BK Detail";

    if (!g_pong.initialized) pong_reset_state();
    pong_update();
    game_draw_arena(ox, "Pong", footer);
    snprintf(score_text, sizeof(score_text), "P1:%u P2:%u", (unsigned)g_pong.player_score, (unsigned)g_pong.ai_score);
    display_draw_text_right_5x7(ox + 118, 2, score_text, true);
    display_draw_vline(ox + GAME_ARENA_X + (GAME_ARENA_W / 2), GAME_ARENA_Y + 3, GAME_ARENA_H - 6, true);
    display_fill_rect(ox + GAME_ARENA_X + 4, g_pong.player_y, 3, 10, true);
    display_fill_rect(ox + GAME_ARENA_X + GAME_ARENA_W - 7, g_pong.ai_y, 3, 10, true);
    display_fill_circle(ox + g_pong.ball_x, g_pong.ball_y, 1, true);
    if (g_pong.mode == GAME_MODE_READY) {
        game_draw_center_notice(ox, 84, (g_pong.player_score == 4U || g_pong.ai_score == 4U) ? "MATCH POINT" : "PRESS OK",
                                (g_pong.player_score == 4U || g_pong.ai_score == 4U) ? "ONE POINT LEFT" : NULL);
    } else if (g_pong.mode == GAME_MODE_GAME_OVER) {
        game_report_result(GAME_ID_PONG, g_pong.final_score, &g_pong.new_high, &g_pong.result_reported);
        game_draw_result_overlay(ox, "AI WINS", g_pong.final_score, g_pong.new_high, NULL);
    } else if (g_pong.mode == GAME_MODE_WIN) {
        game_report_result(GAME_ID_PONG, g_pong.final_score, &g_pong.new_high, &g_pong.result_reported);
        game_draw_result_overlay(ox, "YOU WIN", g_pong.final_score, g_pong.new_high, NULL);
    }
}

static bool pong_handle_internal(const KeyEvent *e, uint32_t now_ms)
{
    if (e == NULL) return false;
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_UP) {
        pong_move_player(-1);
        return true;
    }
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_DOWN) {
        pong_move_player(1);
        return true;
    }
    if (e->type != KEY_EVENT_SHORT) return false;
    if (e->id == KEY_ID_BACK) {
        game_go_detail(now_ms);
        return true;
    }
    if (e->id == KEY_ID_OK) {
        pong_start_round();
        return true;
    }
    return false;
}

static const uint8_t g_tetris_shapes[7][4][4][4] = {
    {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},{{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},{{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}}},
    {{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}},
    {{{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},{{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},{{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}},
    {{{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},{{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},{{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}},
    {{{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},{{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},{{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}},
    {{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},{{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},{{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}}},
    {{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},{{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}}
};

static void snake_spawn_food(void)
{
    uint16_t attempts = 0U;

    while (attempts < SNAKE_MAX_CELLS) {
        uint8_t x = (uint8_t)(game_rand_next(&g_snake.seed) % SNAKE_COLS);
        uint8_t y = (uint8_t)(game_rand_next(&g_snake.seed) % SNAKE_ROWS);
        bool occupied = false;

        for (uint16_t i = 0U; i < g_snake.length; ++i) {
            if (g_snake.body_x[i] == x && g_snake.body_y[i] == y) {
                occupied = true;
                break;
            }
        }
        if (!occupied) {
            g_snake.food_x = x;
            g_snake.food_y = y;
            g_snake.food_type = (uint8_t)((g_snake.score >= 3U && (game_rand_next(&g_snake.seed) % 5U) == 0U) ? 2U : 1U);
            g_snake.bonus_until_ms = g_snake.food_type == 2U ? (game_now_ms() + 2500U) : 0U;
            return;
        }
        attempts++;
    }
    g_snake.food_x = 0U;
    g_snake.food_y = 0U;
}

static void snake_reset_state(void)
{
    memset(&g_snake, 0, sizeof(g_snake));
    g_snake.initialized = true;
    g_snake.mode = GAME_MODE_READY;
    g_snake.last_step_ms = game_now_ms();
    g_snake.seed = (uint16_t)g_snake.last_step_ms;
    g_snake.length = 4U;
    g_snake.dir = DIR_RIGHT;
    g_snake.queued_turn = 0;
    for (uint16_t i = 0U; i < g_snake.length; ++i) {
        g_snake.body_x[i] = (uint8_t)(6U - i);
        g_snake.body_y[i] = 3U;
    }
    snake_spawn_food();
}

static void snake_start(void)
{
    if (g_snake.mode == GAME_MODE_GAME_OVER || g_snake.mode == GAME_MODE_WIN) {
        snake_reset_state();
    }
    g_snake.mode = GAME_MODE_PLAYING;
    g_snake.paused = false;
    g_snake.last_step_ms = game_now_ms();
}

static void snake_turn_left(void)
{
    g_snake.queued_turn = -1;
}

static void snake_turn_right(void)
{
    g_snake.queued_turn = 1;
}

static void snake_apply_queued_turn(void)
{
    if (g_snake.queued_turn < 0) {
        if (g_snake.dir == DIR_UP) g_snake.dir = DIR_LEFT;
        else if (g_snake.dir == DIR_LEFT) g_snake.dir = DIR_DOWN;
        else if (g_snake.dir == DIR_DOWN) g_snake.dir = DIR_RIGHT;
        else g_snake.dir = DIR_UP;
    } else if (g_snake.queued_turn > 0) {
        if (g_snake.dir == DIR_UP) g_snake.dir = DIR_RIGHT;
        else if (g_snake.dir == DIR_RIGHT) g_snake.dir = DIR_DOWN;
        else if (g_snake.dir == DIR_DOWN) g_snake.dir = DIR_LEFT;
        else g_snake.dir = DIR_UP;
    }
    g_snake.queued_turn = 0;
}

static void snake_update(void)
{
    uint32_t interval_ms = 240U;
    int16_t next_x;
    int16_t next_y;
    bool ate_food;

    if (g_snake.length >= 20U) interval_ms = 135U;
    else if (g_snake.length >= 14U) interval_ms = 165U;
    else if (g_snake.length >= 9U) interval_ms = 195U;

    if (g_snake.mode != GAME_MODE_PLAYING || g_snake.paused || !game_should_tick(&g_snake.last_step_ms, interval_ms)) return;
    snake_apply_queued_turn();

    next_x = g_snake.body_x[0];
    next_y = g_snake.body_y[0];
    if (g_snake.dir == DIR_UP) next_y--;
    else if (g_snake.dir == DIR_RIGHT) next_x++;
    else if (g_snake.dir == DIR_DOWN) next_y++;
    else next_x--;

    if (next_x < 0 || next_x >= SNAKE_COLS || next_y < 0 || next_y >= SNAKE_ROWS) {
        g_snake.final_score = (uint16_t)(g_snake.score * 10U + g_snake.length);
        g_snake.mode = GAME_MODE_GAME_OVER;
        return;
    }
    for (uint16_t i = 0U; i < g_snake.length; ++i) {
        if (g_snake.body_x[i] == (uint8_t)next_x && g_snake.body_y[i] == (uint8_t)next_y) {
            g_snake.final_score = (uint16_t)(g_snake.score * 10U + g_snake.length);
            g_snake.mode = GAME_MODE_GAME_OVER;
            return;
        }
    }

    if (g_snake.food_type == 2U && g_snake.bonus_until_ms != 0U && game_now_ms() > g_snake.bonus_until_ms) {
        snake_spawn_food();
    }
    ate_food = next_x == g_snake.food_x && next_y == g_snake.food_y;
    if (!ate_food) {
        for (uint16_t i = g_snake.length - 1U; i > 0U; --i) {
            g_snake.body_x[i] = g_snake.body_x[i - 1U];
            g_snake.body_y[i] = g_snake.body_y[i - 1U];
        }
    } else if (g_snake.length < SNAKE_MAX_CELLS) {
        for (uint16_t i = g_snake.length; i > 0U; --i) {
            g_snake.body_x[i] = g_snake.body_x[i - 1U];
            g_snake.body_y[i] = g_snake.body_y[i - 1U];
        }
        g_snake.length++;
    }

    g_snake.body_x[0] = (uint8_t)next_x;
    g_snake.body_y[0] = (uint8_t)next_y;
    if (ate_food) {
        g_snake.score = (uint16_t)(g_snake.score + (g_snake.food_type == 2U ? 4U : 1U));
        if (g_snake.length >= SNAKE_MAX_CELLS) {
            g_snake.final_score = (uint16_t)(g_snake.score * 10U + g_snake.length);
            g_snake.mode = GAME_MODE_WIN;
        } else {
            snake_spawn_food();
        }
    }
}

static void snake_render_internal(int16_t ox)
{
    char status[20];
    char detail[18];
    int16_t grid_x = ox + 10;
    int16_t grid_y = 16;

    if (!g_snake.initialized) snake_reset_state();
    snake_update();
    game_draw_arena(ox, "Snake",
                    g_snake.mode == GAME_MODE_PLAYING ? "UP/DN Turn  BK Pause" :
                    ((g_snake.mode == GAME_MODE_GAME_OVER || g_snake.mode == GAME_MODE_WIN) ? "OK Retry  BK Detail" : "OK Start  BK Pause"));
    snprintf(status, sizeof(status), "LEN %u S %u", (unsigned)g_snake.length, (unsigned)g_snake.score);
    display_draw_text_right_5x7(ox + 118, 2, status, true);
    for (uint16_t i = 0U; i < g_snake.length; ++i) {
        int16_t cell_x = grid_x + g_snake.body_x[i] * SNAKE_CELL_SIZE;
        int16_t cell_y = grid_y + g_snake.body_y[i] * SNAKE_CELL_SIZE;

        display_fill_rect(cell_x, cell_y, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1, true);
        if (i == 0U) display_draw_pixel(cell_x + 1, cell_y + 1, false);
    }
    display_fill_rect(grid_x + g_snake.food_x * SNAKE_CELL_SIZE, grid_y + g_snake.food_y * SNAKE_CELL_SIZE, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1, true);
    if (g_snake.food_type == 2U) {
        if (((game_now_ms() / 150U) & 0x01U) == 0U) {
            display_draw_rect(grid_x + g_snake.food_x * SNAKE_CELL_SIZE, grid_y + g_snake.food_y * SNAKE_CELL_SIZE, SNAKE_CELL_SIZE - 1, SNAKE_CELL_SIZE - 1, false);
        }
    } else {
        display_fill_rect(grid_x + g_snake.food_x * SNAKE_CELL_SIZE + 1, grid_y + g_snake.food_y * SNAKE_CELL_SIZE + 1, SNAKE_CELL_SIZE - 3, SNAKE_CELL_SIZE - 3, false);
    }
    if (g_snake.mode == GAME_MODE_READY) {
        game_draw_center_notice(ox, 82, "PRESS OK", "BONUS BLINKS");
    } else if (g_snake.paused) {
        game_draw_center_notice(ox, 70, "PAUSE", "OK RESUME");
    } else if (g_snake.mode == GAME_MODE_GAME_OVER) {
        game_report_result(GAME_ID_SNAKE, g_snake.final_score, &g_snake.new_high, &g_snake.result_reported);
        snprintf(detail, sizeof(detail), "LEN %u", (unsigned)g_snake.length);
        game_draw_result_overlay(ox, "SNAKE DOWN", g_snake.final_score, g_snake.new_high, detail);
    } else if (g_snake.mode == GAME_MODE_WIN) {
        game_report_result(GAME_ID_SNAKE, g_snake.final_score, &g_snake.new_high, &g_snake.result_reported);
        snprintf(detail, sizeof(detail), "LEN %u", (unsigned)g_snake.length);
        game_draw_result_overlay(ox, "FULL GRID", g_snake.final_score, g_snake.new_high, detail);
    }
}

static bool snake_handle_internal(const KeyEvent *e, uint32_t now_ms)
{
    if (e == NULL) return false;
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_UP) {
        if (g_snake.mode == GAME_MODE_PLAYING && !g_snake.paused) snake_turn_left();
        return true;
    }
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_DOWN) {
        if (g_snake.mode == GAME_MODE_PLAYING && !g_snake.paused) snake_turn_right();
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_OK) {
        if (g_snake.mode != GAME_MODE_PLAYING || g_snake.paused) snake_start();
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        if (g_snake.mode == GAME_MODE_PLAYING) {
            g_snake.paused = !g_snake.paused;
            g_snake.last_step_ms = game_now_ms();
        } else if (g_snake.mode == GAME_MODE_GAME_OVER || g_snake.mode == GAME_MODE_WIN) {
            game_go_detail(now_ms);
        }
        return true;
    }
    if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_BACK) {
        game_go_detail(now_ms);
        return true;
    }
    return false;
}

static void tetris_refill_bag(void)
{
    for (uint8_t i = 0U; i < 7U; ++i) {
        g_tetris.bag[i] = i;
    }
    for (uint8_t i = 0U; i < 7U; ++i) {
        uint8_t j = (uint8_t)(game_rand_next(&g_tetris.seed) % 7U);
        uint8_t tmp = g_tetris.bag[i];
        g_tetris.bag[i] = g_tetris.bag[j];
        g_tetris.bag[j] = tmp;
    }
    g_tetris.bag_index = 0U;
}

static uint8_t tetris_random_piece(void)
{
    if (g_tetris.bag_index >= 7U) {
        tetris_refill_bag();
    }
    return g_tetris.bag[g_tetris.bag_index++];
}

static bool tetris_cell_filled(uint8_t piece, uint8_t rotation, uint8_t y, uint8_t x)
{
    return g_tetris_shapes[piece % 7U][rotation % 4U][y % 4U][x % 4U] != 0U;
}

static bool tetris_can_place(uint8_t piece, uint8_t rotation, int8_t base_x, int8_t base_y)
{
    for (uint8_t y = 0U; y < 4U; ++y) {
        for (uint8_t x = 0U; x < 4U; ++x) {
            int8_t board_x;
            int8_t board_y;

            if (!tetris_cell_filled(piece, rotation, y, x)) continue;
            board_x = (int8_t)(base_x + (int8_t)x);
            board_y = (int8_t)(base_y + (int8_t)y);
            if (board_x < 0 || board_x >= TETRIS_COLS || board_y < 0 || board_y >= TETRIS_ROWS) return false;
            if (g_tetris.board[board_y][board_x] != 0U) return false;
        }
    }
    return true;
}

static void tetris_spawn_piece(void)
{
    g_tetris.piece = g_tetris.next_piece;
    g_tetris.next_piece = tetris_random_piece();
    g_tetris.rotation = 0U;
    g_tetris.piece_x = 3;
    g_tetris.piece_y = 0;
    if (!tetris_can_place(g_tetris.piece, g_tetris.rotation, g_tetris.piece_x, g_tetris.piece_y)) { g_tetris.final_score = g_tetris.score; g_tetris.mode = GAME_MODE_GAME_OVER; }
}

static void tetris_reset_board(void)
{
    memset(g_tetris.board, 0, sizeof(g_tetris.board));
    g_tetris.score = 0U;
    g_tetris.lines = 0U;
    g_tetris.level = 1U;
    g_tetris.paused = false;
    tetris_refill_bag();
    g_tetris.next_piece = tetris_random_piece();
    tetris_spawn_piece();
}

static void tetris_reset_state(void)
{
    memset(&g_tetris, 0, sizeof(g_tetris));
    g_tetris.initialized = true;
    g_tetris.mode = GAME_MODE_READY;
    g_tetris.last_step_ms = game_now_ms();
    g_tetris.seed = (uint16_t)g_tetris.last_step_ms;
    tetris_refill_bag();
    g_tetris.next_piece = tetris_random_piece();
    g_tetris.level = 1U;
}

static void tetris_lock_piece(void)
{
    for (uint8_t y = 0U; y < 4U; ++y) {
        for (uint8_t x = 0U; x < 4U; ++x) {
            int8_t board_x;
            int8_t board_y;

            if (!tetris_cell_filled(g_tetris.piece, g_tetris.rotation, y, x)) continue;
            board_x = (int8_t)(g_tetris.piece_x + (int8_t)x);
            board_y = (int8_t)(g_tetris.piece_y + (int8_t)y);
            if (board_x >= 0 && board_x < TETRIS_COLS && board_y >= 0 && board_y < TETRIS_ROWS) {
                g_tetris.board[board_y][board_x] = (uint8_t)(g_tetris.piece + 1U);
            }
        }
    }
}

static void tetris_clear_lines(void)
{
    uint8_t cleared = 0U;

    for (int8_t y = TETRIS_ROWS - 1; y >= 0; --y) {
        bool full = true;

        for (uint8_t x = 0U; x < TETRIS_COLS; ++x) {
            if (g_tetris.board[y][x] == 0U) {
                full = false;
                break;
            }
        }
        if (!full) continue;
        cleared++;
        for (int8_t row = y; row > 0; --row) memcpy(g_tetris.board[row], g_tetris.board[row - 1], sizeof(g_tetris.board[row]));
        memset(g_tetris.board[0], 0, sizeof(g_tetris.board[0]));
        y++;
    }
    if (cleared > 0U) {
        uint16_t add = 0U;

        g_tetris.lines = (uint16_t)(g_tetris.lines + cleared);
        g_tetris.level = (uint8_t)(1U + (g_tetris.lines / 8U));
        if (cleared == 1U) add = 100U;
        else if (cleared == 2U) add = 300U;
        else if (cleared == 3U) add = 500U;
        else add = 800U;
        g_tetris.score = (uint16_t)(g_tetris.score + (uint16_t)(add * g_tetris.level));
    }
}

static void tetris_start_game(void)
{
    if (g_tetris.mode == GAME_MODE_GAME_OVER || g_tetris.mode == GAME_MODE_READY) tetris_reset_board();
    g_tetris.mode = GAME_MODE_PLAYING;
    g_tetris.paused = false;
    g_tetris.last_step_ms = game_now_ms();
}

static void tetris_move_piece(int8_t dx)
{
    int8_t next_x = (int8_t)(g_tetris.piece_x + dx);

    if (g_tetris.mode == GAME_MODE_PLAYING && !g_tetris.paused &&
        tetris_can_place(g_tetris.piece, g_tetris.rotation, next_x, g_tetris.piece_y)) {
        g_tetris.piece_x = next_x;
    }
}

static void tetris_rotate_piece(void)
{
    uint8_t next_rot = (uint8_t)((g_tetris.rotation + 1U) % 4U);

    if (g_tetris.mode == GAME_MODE_PLAYING && !g_tetris.paused) {
        static const int8_t kicks[] = {0, -1, 1, -2, 2};

        for (uint8_t i = 0U; i < (sizeof(kicks) / sizeof(kicks[0])); ++i) {
            int8_t next_x = (int8_t)(g_tetris.piece_x + kicks[i]);

            if (tetris_can_place(g_tetris.piece, next_rot, next_x, g_tetris.piece_y)) {
                g_tetris.piece_x = next_x;
                g_tetris.rotation = next_rot;
                break;
            }
        }
    }
}

static void tetris_hard_drop(void)
{
    if (g_tetris.mode != GAME_MODE_PLAYING || g_tetris.paused) return;
    while (tetris_can_place(g_tetris.piece, g_tetris.rotation, g_tetris.piece_x, (int8_t)(g_tetris.piece_y + 1))) g_tetris.piece_y++;
    tetris_lock_piece();
    tetris_clear_lines();
    tetris_spawn_piece();
}

static void tetris_update(void)
{
    uint32_t interval_ms = 520U;

    if (g_tetris.level >= 8U) interval_ms = 170U;
    else if (g_tetris.level >= 6U) interval_ms = 220U;
    else if (g_tetris.level >= 4U) interval_ms = 290U;
    else if (g_tetris.level >= 2U) interval_ms = 390U;

    if (g_tetris.mode != GAME_MODE_PLAYING || g_tetris.paused || !game_should_tick(&g_tetris.last_step_ms, interval_ms)) return;
    if (tetris_can_place(g_tetris.piece, g_tetris.rotation, g_tetris.piece_x, (int8_t)(g_tetris.piece_y + 1))) g_tetris.piece_y++;
    else {
        tetris_lock_piece();
        tetris_clear_lines();
        tetris_spawn_piece();
    }
}

static void tetris_draw_piece_preview(int16_t ox, int16_t base_x, int16_t base_y, uint8_t piece)
{
    for (uint8_t y = 0U; y < 4U; ++y) {
        for (uint8_t x = 0U; x < 4U; ++x) {
            if (tetris_cell_filled(piece, 0U, y, x)) display_fill_rect(ox + base_x + x * 2, base_y + y * 2, 2, 2, true);
        }
    }
}

static void tetris_render_internal(int16_t ox)
{
    char text[18];
    int16_t board_x = ox + 12;
    int16_t board_y = 16;

    if (!g_tetris.initialized) tetris_reset_state();
    tetris_update();
    game_draw_arena(ox, "Tetris",
                    g_tetris.mode == GAME_MODE_PLAYING ? "UP/DN Move OK Rot" :
                    (g_tetris.mode == GAME_MODE_GAME_OVER ? "OK Retry  BK Detail" : "OK Start  BK Pause"));
    display_draw_rect(board_x - 1, board_y - 1, TETRIS_COLS * TETRIS_CELL_SIZE + 2, TETRIS_ROWS * TETRIS_CELL_SIZE + 2, true);
    for (uint8_t y = 0U; y < TETRIS_ROWS; ++y) {
        for (uint8_t x = 0U; x < TETRIS_COLS; ++x) {
            if (g_tetris.board[y][x] != 0U) display_fill_rect(board_x + x * TETRIS_CELL_SIZE, board_y + y * TETRIS_CELL_SIZE, TETRIS_CELL_SIZE, TETRIS_CELL_SIZE, true);
        }
    }
    if (g_tetris.mode == GAME_MODE_PLAYING || g_tetris.mode == GAME_MODE_READY) {
        for (uint8_t y = 0U; y < 4U; ++y) {
            for (uint8_t x = 0U; x < 4U; ++x) {
                if (tetris_cell_filled(g_tetris.piece, g_tetris.rotation, y, x)) {
                    display_fill_rect(board_x + (g_tetris.piece_x + x) * TETRIS_CELL_SIZE, board_y + (g_tetris.piece_y + y) * TETRIS_CELL_SIZE, TETRIS_CELL_SIZE, TETRIS_CELL_SIZE, true);
                }
            }
        }
    }
    display_draw_text_5x7(ox + 42, 18, "NEXT", true);
    tetris_draw_piece_preview(0, ox + 46, 27, g_tetris.next_piece);
    snprintf(text, sizeof(text), "L %u", (unsigned)g_tetris.lines);
    display_draw_text_5x7(ox + 72, 18, text, true);
    snprintf(text, sizeof(text), "LV %u", (unsigned)g_tetris.level);
    display_draw_text_5x7(ox + 72, 28, text, true);
    snprintf(text, sizeof(text), "HI %u", (unsigned)model_get_game_high_score(GAME_ID_TETRIS));
    display_draw_text_5x7(ox + 72, 38, text, true);
    if (g_tetris.mode == GAME_MODE_READY) {
        game_draw_center_notice(ox, 70, "PRESS OK", "HOLD OK DROP");
    } else if (g_tetris.paused) {
        game_draw_center_notice(ox, 62, "PAUSE", "BK TO EXIT");
    } else if (g_tetris.mode == GAME_MODE_GAME_OVER) {
        game_report_result(GAME_ID_TETRIS, g_tetris.final_score, &g_tetris.new_high, &g_tetris.result_reported);
        game_draw_result_overlay(ox, "STACK OVER", g_tetris.final_score, g_tetris.new_high, NULL);
    }
}

static bool tetris_handle_internal(const KeyEvent *e, uint32_t now_ms)
{
    if (e == NULL) return false;
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_UP) {
        tetris_move_piece(-1);
        return true;
    }
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_DOWN) {
        tetris_move_piece(1);
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_OK) {
        if (g_tetris.mode != GAME_MODE_PLAYING || g_tetris.paused) tetris_start_game();
        else tetris_rotate_piece();
        return true;
    }
    if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_OK) {
        tetris_hard_drop();
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        if (g_tetris.mode == GAME_MODE_PLAYING) {
            g_tetris.paused = !g_tetris.paused;
            g_tetris.last_step_ms = game_now_ms();
        } else if (g_tetris.mode == GAME_MODE_GAME_OVER) {
            game_go_detail(now_ms);
        }
        return true;
    }
    if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_BACK) {
        game_go_detail(now_ms);
        return true;
    }
    return false;
}

static void shooter_reset_state(void)
{
    memset(&g_shooter, 0, sizeof(g_shooter));
    g_shooter.initialized = true;
    g_shooter.mode = GAME_MODE_READY;
    g_shooter.last_step_ms = game_now_ms();
    g_shooter.last_spawn_ms = g_shooter.last_step_ms;
    g_shooter.last_fire_ms = g_shooter.last_step_ms;
    g_shooter.seed = (uint16_t)g_shooter.last_step_ms;
    g_shooter.player_y = GAME_ARENA_Y + 16;
    for (uint8_t i = 0U; i < SHOOTER_MAX_STARS; ++i) {
        g_shooter.stars[i].x = (int16_t)(GAME_ARENA_X + 10 + (i * 17));
        g_shooter.stars[i].y = (int16_t)(GAME_ARENA_Y + 4 + (i * 4));
    }
}

static void shooter_start(void)
{
    if (g_shooter.mode != GAME_MODE_PLAYING) shooter_reset_state();
    g_shooter.mode = GAME_MODE_PLAYING;
    g_shooter.last_step_ms = game_now_ms();
    g_shooter.last_spawn_ms = g_shooter.last_step_ms;
    g_shooter.invulnerable_until_ms = g_shooter.last_step_ms + GAME_RESULT_HOLD_MS;
}

static void shooter_fire(void)
{
    uint32_t now_ms = game_now_ms();

    if (g_shooter.mode != GAME_MODE_PLAYING || (now_ms - g_shooter.last_fire_ms) < 140U) return;
    for (uint8_t i = 0U; i < SHOOTER_MAX_BULLETS; ++i) {
        if (!g_shooter.bullets[i].active) {
            g_shooter.bullets[i].active = true;
            g_shooter.bullets[i].x = GAME_ARENA_X + 18;
            g_shooter.bullets[i].y = g_shooter.player_y + 2;
            g_shooter.last_fire_ms = now_ms;
            break;
        }
    }
}

static void shooter_spawn_enemy(void)
{
    for (uint8_t i = 0U; i < SHOOTER_MAX_ENEMIES; ++i) {
        if (!g_shooter.enemies[i].active) {
            g_shooter.enemies[i].active = true;
            g_shooter.enemies[i].x = GAME_ARENA_X + GAME_ARENA_W - 8;
            g_shooter.enemies[i].y = (int16_t)(GAME_ARENA_Y + 3 + (game_rand_next(&g_shooter.seed) % (GAME_ARENA_H - 10)));
            g_shooter.enemies[i].speed = (int8_t)game_clamp_u16((uint16_t)(1U + (g_shooter.score / 8U)), 1U, 3U);
            g_shooter.enemies[i].type = (uint8_t)((g_shooter.score >= 8U && (game_rand_next(&g_shooter.seed) % 3U) == 0U) ? 1U : 0U);
            g_shooter.enemies[i].phase = (int8_t)(game_rand_next(&g_shooter.seed) & 0x03U);
            return;
        }
    }
}

static void shooter_note_fault(void)
{
    g_shooter.invulnerable_until_ms = game_now_ms() + 700U;
    g_shooter.faults++;
    if (g_shooter.faults >= 3U) {
        g_shooter.final_score = g_shooter.score;
        g_shooter.mode = GAME_MODE_GAME_OVER;
    }
}

static void shooter_update(void)
{
    uint32_t now_ms = game_now_ms();
    uint32_t spawn_interval = 320U;

    if ((g_shooter.score / 6U) % 3U == 2U) {
        spawn_interval = 420U;
    } else if (g_shooter.score >= 18U) {
        spawn_interval = 180U;
    } else if (g_shooter.score >= 8U) {
        spawn_interval = 230U;
    }

    if (g_shooter.mode != GAME_MODE_PLAYING || !game_should_tick(&g_shooter.last_step_ms, 50U)) return;
    for (uint8_t i = 0U; i < SHOOTER_MAX_STARS; ++i) {
        g_shooter.stars[i].x--;
        if (g_shooter.stars[i].x < GAME_ARENA_X + 2) {
            g_shooter.stars[i].x = GAME_ARENA_X + GAME_ARENA_W - 3;
            g_shooter.stars[i].y = (int16_t)(GAME_ARENA_Y + 2 + (game_rand_next(&g_shooter.seed) % (GAME_ARENA_H - 4)));
        }
    }
    if ((now_ms - g_shooter.last_spawn_ms) >= spawn_interval) {
        shooter_spawn_enemy();
        g_shooter.last_spawn_ms = now_ms;
    }
    for (uint8_t i = 0U; i < SHOOTER_MAX_BULLETS; ++i) {
        if (!g_shooter.bullets[i].active) continue;
        g_shooter.bullets[i].x += 4;
        if (g_shooter.bullets[i].x > GAME_ARENA_X + GAME_ARENA_W - 2) g_shooter.bullets[i].active = false;
    }
    for (uint8_t i = 0U; i < SHOOTER_MAX_ENEMIES; ++i) {
        ShooterEnemy *enemy = &g_shooter.enemies[i];

        if (!enemy->active) continue;
        enemy->x -= enemy->speed;
        if (enemy->type == 1U) {
            enemy->phase = (int8_t)((enemy->phase + 1) & 0x03);
            if (enemy->phase == 0) enemy->y = game_clamp_i16((int16_t)(enemy->y - 1), GAME_ARENA_Y + 2, GAME_ARENA_Y + GAME_ARENA_H - 8);
            else if (enemy->phase == 2) enemy->y = game_clamp_i16((int16_t)(enemy->y + 1), GAME_ARENA_Y + 2, GAME_ARENA_Y + GAME_ARENA_H - 8);
        }
        if (now_ms >= g_shooter.invulnerable_until_ms &&
            game_rect_overlap(GAME_ARENA_X + 10, g_shooter.player_y, 7, 6, enemy->x, enemy->y, 6, 5)) {
            enemy->active = false;
            shooter_note_fault();
            continue;
        }
        for (uint8_t j = 0U; j < SHOOTER_MAX_BULLETS; ++j) {
            if (g_shooter.bullets[j].active && game_rect_overlap(g_shooter.bullets[j].x, g_shooter.bullets[j].y, 3, 1, enemy->x, enemy->y, 6, 5)) {
                g_shooter.bullets[j].active = false;
                enemy->active = false;
                g_shooter.score++;
                break;
            }
        }
        if (enemy->active && enemy->x < GAME_ARENA_X + 2) {
            enemy->active = false;
            if (now_ms >= g_shooter.invulnerable_until_ms) {
                shooter_note_fault();
            }
        }
    }
}

static void shooter_render_ship(int16_t ox, int16_t x, int16_t y)
{
    display_draw_pixel(ox + x, y + 2, true);
    display_fill_rect(ox + x + 1, y + 1, 4, 4, true);
    display_draw_pixel(ox + x + 5, y + 2, true);
}

static void shooter_render_internal(int16_t ox)
{
    char status[20];
    const char *footer = (g_shooter.mode == GAME_MODE_PLAYING) ? "UP/DN Move OK Fire" : "OK Start  BK Detail";

    if (!g_shooter.initialized) shooter_reset_state();
    shooter_update();
    game_draw_arena(ox, "Shooter", footer);
    snprintf(status, sizeof(status), "S%u LIFE %u", (unsigned)g_shooter.score, (unsigned)(3U - g_shooter.faults));
    display_draw_text_right_5x7(ox + 118, 2, status, true);
    for (uint8_t i = 0U; i < SHOOTER_MAX_STARS; ++i) display_draw_pixel(ox + g_shooter.stars[i].x, g_shooter.stars[i].y, true);
    shooter_render_ship(ox, GAME_ARENA_X + 10, g_shooter.player_y);
    if (g_shooter.invulnerable_until_ms > game_now_ms() && ((game_now_ms() / 100U) & 0x01U) == 0U) {
        display_draw_round_rect(ox + GAME_ARENA_X + 8, g_shooter.player_y - 1, 11, 8, true);
    }
    for (uint8_t i = 0U; i < SHOOTER_MAX_BULLETS; ++i) {
        if (g_shooter.bullets[i].active) display_draw_hline(ox + g_shooter.bullets[i].x, g_shooter.bullets[i].y, 3, true);
    }
    for (uint8_t i = 0U; i < SHOOTER_MAX_ENEMIES; ++i) {
        if (g_shooter.enemies[i].active) {
            display_fill_rect(ox + g_shooter.enemies[i].x, g_shooter.enemies[i].y, 6, 5, true);
            display_draw_pixel(ox + g_shooter.enemies[i].x + 1, g_shooter.enemies[i].y + 1, false);
            display_draw_pixel(ox + g_shooter.enemies[i].x + 4, g_shooter.enemies[i].y + 1, false);
            if (g_shooter.enemies[i].type == 1U) display_draw_pixel(ox + g_shooter.enemies[i].x + 2, g_shooter.enemies[i].y + 4, false);
        }
    }
    if (g_shooter.mode == GAME_MODE_READY) {
        game_draw_center_notice(ox, 82, "PRESS OK", "WAVE INBOUND");
    } else if (g_shooter.mode == GAME_MODE_GAME_OVER) {
        game_report_result(GAME_ID_SHOOTER, g_shooter.final_score, &g_shooter.new_high, &g_shooter.result_reported);
        game_draw_result_overlay(ox, "SECTOR LOST", g_shooter.final_score, g_shooter.new_high, NULL);
    }
}

static bool shooter_handle_internal(const KeyEvent *e, uint32_t now_ms)
{
    if (e == NULL) return false;
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_UP) {
        if (g_shooter.mode == GAME_MODE_PLAYING) g_shooter.player_y = game_clamp_i16((int16_t)(g_shooter.player_y - 3), GAME_ARENA_Y + 2, GAME_ARENA_Y + GAME_ARENA_H - 8);
        return true;
    }
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_DOWN) {
        if (g_shooter.mode == GAME_MODE_PLAYING) g_shooter.player_y = game_clamp_i16((int16_t)(g_shooter.player_y + 3), GAME_ARENA_Y + 2, GAME_ARENA_Y + GAME_ARENA_H - 8);
        return true;
    }
    if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_REPEAT) && e->id == KEY_ID_OK) {
        if (g_shooter.mode != GAME_MODE_PLAYING) shooter_start();
        else shooter_fire();
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        game_go_detail(now_ms);
        return true;
    }
    return false;
}

bool ui_page_game_is_page(PageId page)
{
    return page == PAGE_BREAKOUT || page == PAGE_DINO || page == PAGE_PONG ||
           page == PAGE_SNAKE || page == PAGE_TETRIS || page == PAGE_SHOOTER;
}

void ui_page_game_reset(PageId page)
{
    if (page == PAGE_BREAKOUT) breakout_reset_state();
    else if (page == PAGE_DINO) dino_reset_state();
    else if (page == PAGE_PONG) pong_reset_state();
    else if (page == PAGE_SNAKE) snake_reset_state();
    else if (page == PAGE_TETRIS) tetris_reset_state();
    else if (page == PAGE_SHOOTER) shooter_reset_state();
}

void ui_page_breakout_render(PageId page, int16_t ox) { (void)page; breakout_render_internal(ox); }
bool ui_page_breakout_handle(PageId page, const KeyEvent *e, uint32_t now_ms) { (void)page; return breakout_handle_internal(e, now_ms); }
void ui_page_dino_render(PageId page, int16_t ox) { (void)page; dino_render_internal(ox); }
bool ui_page_dino_handle(PageId page, const KeyEvent *e, uint32_t now_ms) { (void)page; return dino_handle_internal(e, now_ms); }
void ui_page_pong_render(PageId page, int16_t ox) { (void)page; pong_render_internal(ox); }
bool ui_page_pong_handle(PageId page, const KeyEvent *e, uint32_t now_ms) { (void)page; return pong_handle_internal(e, now_ms); }
void ui_page_snake_render(PageId page, int16_t ox) { (void)page; snake_render_internal(ox); }
bool ui_page_snake_handle(PageId page, const KeyEvent *e, uint32_t now_ms) { (void)page; return snake_handle_internal(e, now_ms); }
void ui_page_tetris_render(PageId page, int16_t ox) { (void)page; tetris_render_internal(ox); }
bool ui_page_tetris_handle(PageId page, const KeyEvent *e, uint32_t now_ms) { (void)page; return tetris_handle_internal(e, now_ms); }
void ui_page_shooter_render(PageId page, int16_t ox) { (void)page; shooter_render_internal(ox); }
bool ui_page_shooter_handle(PageId page, const KeyEvent *e, uint32_t now_ms) { (void)page; return shooter_handle_internal(e, now_ms); }






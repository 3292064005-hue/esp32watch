#ifndef UI_PAGE_GAMES_SUPPORT_H
#define UI_PAGE_GAMES_SUPPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "model.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_ARENA_X 8
#define GAME_ARENA_Y 14
#define GAME_ARENA_W 112
#define GAME_ARENA_H 36

/**
 * @brief Clamp a signed 16-bit value to a closed range.
 *
 * @param[in] value Candidate value.
 * @param[in] min_value Inclusive lower bound.
 * @param[in] max_value Inclusive upper bound.
 * @return Clamped value in the inclusive range [min_value, max_value].
 * @throws None.
 */
int16_t game_clamp_i16(int16_t value, int16_t min_value, int16_t max_value);

/**
 * @brief Clamp an unsigned 16-bit value to a closed range.
 *
 * @param[in] value Candidate value.
 * @param[in] min_value Inclusive lower bound.
 * @param[in] max_value Inclusive upper bound.
 * @return Clamped value in the inclusive range [min_value, max_value].
 * @throws None.
 */
uint16_t game_clamp_u16(uint16_t value, uint16_t min_value, uint16_t max_value);

/**
 * @brief Test whether two axis-aligned rectangles overlap.
 *
 * @param[in] ax First rectangle X.
 * @param[in] ay First rectangle Y.
 * @param[in] aw First rectangle width.
 * @param[in] ah First rectangle height.
 * @param[in] bx Second rectangle X.
 * @param[in] by Second rectangle Y.
 * @param[in] bw Second rectangle width.
 * @param[in] bh Second rectangle height.
 * @return true when the rectangles overlap.
 * @throws None.
 */
bool game_rect_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                       int16_t bx, int16_t by, int16_t bw, int16_t bh);

/**
 * @brief Advance the lightweight deterministic game RNG.
 *
 * @param[in,out] seed RNG state.
 * @return Next pseudo-random value, or 0 when @p seed is NULL.
 * @throws None.
 */
uint16_t game_rand_next(uint16_t *seed);

/**
 * @brief Read the monotonic game timebase in milliseconds.
 *
 * @return Current runtime monotonic time in milliseconds.
 * @throws None.
 */
uint32_t game_now_ms(void);

/**
 * @brief Draw the shared game arena chrome for a page.
 *
 * @param[in] ox Page X offset.
 * @param[in] title Header title.
 * @param[in] footer Footer hint string.
 * @return void.
 * @throws None.
 */
void game_draw_arena(int16_t ox, const char *title, const char *footer);

/**
 * @brief Gate periodic simulation ticks against render transitions and elapsed time.
 *
 * @param[in,out] last_ms Timestamp of the previous accepted tick.
 * @param[in] interval_ms Required minimum interval.
 * @return true when the caller should execute a simulation step.
 * @throws None.
 * @boundary_behavior Resets @p last_ms during transition renders so gameplay does not fast-forward after UI transitions.
 */
bool game_should_tick(uint32_t *last_ms, uint32_t interval_ms);

/**
 * @brief Check whether a ready/countdown deadline has elapsed.
 *
 * @param[in] ready_until_ms Deadline timestamp.
 * @return true when the deadline is non-zero and in the past.
 * @throws None.
 */
bool game_ready_elapsed(uint32_t ready_until_ms);

/**
 * @brief Draw a centered notice box inside the game arena.
 *
 * @param[in] ox Page X offset.
 * @param[in] width Notice width.
 * @param[in] line_a Primary line.
 * @param[in] line_b Optional secondary line.
 * @return void.
 * @throws None.
 */
void game_draw_center_notice(int16_t ox, int16_t width, const char *line_a, const char *line_b);

/**
 * @brief Draw the shared result overlay used by core game pages.
 *
 * @param[in] ox Page X offset.
 * @param[in] title Result title.
 * @param[in] score Final score.
 * @param[in] new_high Whether the score is a new high score.
 * @param[in] detail Optional detail line.
 * @return void.
 * @throws None.
 */
void game_draw_result_overlay(int16_t ox, const char *title, uint16_t score, bool new_high, const char *detail);

/**
 * @brief Persist and mark a game's final result exactly once.
 *
 * @param[in] game_id Game identifier.
 * @param[in] score Final score.
 * @param[out] new_high Whether the score beat the previous best.
 * @param[in,out] reported Idempotence guard for one-shot reporting.
 * @return void.
 * @throws None.
 * @boundary_behavior No-op when pointers are NULL or the result was already reported.
 */
void game_report_result(GameId game_id, uint16_t score, bool *new_high, bool *reported);

/**
 * @brief Navigate from a game tile into the game detail page.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return void.
 * @throws None.
 */
void game_go_detail(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

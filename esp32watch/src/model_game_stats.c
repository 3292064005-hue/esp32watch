#include "model_internal.h"
#include <string.h>

static uint16_t *model_game_high_score_slot(GameId game_id)
{
    switch (game_id) {
        case GAME_ID_BREAKOUT: return &g_model_domain_state.game_stats.breakout_hi;
        case GAME_ID_DINO: return &g_model_domain_state.game_stats.dino_hi;
        case GAME_ID_PONG: return &g_model_domain_state.game_stats.pong_hi;
        case GAME_ID_SNAKE: return &g_model_domain_state.game_stats.snake_hi;
        case GAME_ID_TETRIS: return &g_model_domain_state.game_stats.tetris_hi;
        case GAME_ID_SHOOTER: return &g_model_domain_state.game_stats.shooter_hi;
        default: return NULL;
    }
}

uint16_t model_get_game_high_score(GameId game_id)
{
    uint16_t *slot = model_game_high_score_slot(game_id);

    return slot != NULL ? *slot : 0U;
}

bool model_set_game_high_score(GameId game_id, uint16_t score)
{
    uint16_t *slot = model_game_high_score_slot(game_id);

    if (slot == NULL || score <= *slot) {
        return false;
    }

    *slot = score;
    model_internal_persist_game_stats();
    model_internal_commit_domain_mutation_with_flags(MODEL_PROJECTION_DIRTY_DOMAIN | MODEL_PROJECTION_DIRTY_UI);
    return true;
}

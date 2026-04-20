#include <string.h>

extern "C" {
#include "persist.h"
#include "main.h"
#include "app_config.h"
#include "persist_codec.h"
}

#include "persist_internal.hpp"

extern "C" void persist_save_game_stats(const GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }
    if (!persist_game_stats_open(false)) {
        return;
    }
    g_game_stats_prefs.putUChar(PERSIST_GAME_STATS_VERSION_KEY, APP_STORAGE_VERSION);
    g_game_stats_prefs.putULong(PERSIST_GAME_BREAKOUT_KEY, stats->breakout_hi);
    g_game_stats_prefs.putULong(PERSIST_GAME_DINO_KEY, stats->dino_hi);
    g_game_stats_prefs.putULong(PERSIST_GAME_PONG_KEY, stats->pong_hi);
    g_game_stats_prefs.putULong(PERSIST_GAME_SNAKE_KEY, stats->snake_hi);
    g_game_stats_prefs.putULong(PERSIST_GAME_TETRIS_KEY, stats->tetris_hi);
    g_game_stats_prefs.putULong(PERSIST_GAME_SHOOTER_KEY, stats->shooter_hi);
    g_game_stats_prefs.end();
}

extern "C" void persist_load_game_stats(GameStatsState *stats)
{
    if (stats == NULL) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    if (!persist_game_stats_open(true)) {
        return;
    }
    if (g_game_stats_prefs.getUChar(PERSIST_GAME_STATS_VERSION_KEY, 0U) != APP_STORAGE_VERSION) {
        g_game_stats_prefs.end();
        return;
    }
    stats->breakout_hi = g_game_stats_prefs.getULong(PERSIST_GAME_BREAKOUT_KEY, 0U);
    stats->dino_hi = g_game_stats_prefs.getULong(PERSIST_GAME_DINO_KEY, 0U);
    stats->pong_hi = g_game_stats_prefs.getULong(PERSIST_GAME_PONG_KEY, 0U);
    stats->snake_hi = g_game_stats_prefs.getULong(PERSIST_GAME_SNAKE_KEY, 0U);
    stats->tetris_hi = g_game_stats_prefs.getULong(PERSIST_GAME_TETRIS_KEY, 0U);
    stats->shooter_hi = g_game_stats_prefs.getULong(PERSIST_GAME_SHOOTER_KEY, 0U);
    g_game_stats_prefs.end();
}

#include "ui_internal.h"

typedef struct {
    GameId game_id;
    PageId page_id;
    const char *name;
    const char *tag;
    const char *desc;
    const char *controls_a;
    const char *controls_b;
} UiGameMeta;

static const UiGameMeta g_games[] = {
    {GAME_ID_BREAKOUT, PAGE_BREAKOUT, "Breakout", "BRICK", "Cut clean angles, guard each life.", "UP/DN Move paddle", "OK Serve or retry"},
    {GAME_ID_DINO, PAGE_DINO, "Dino", "RUN", "Jump clean arcs before gaps get tight.", "OK or UP Jump", "DN Fast drop"},
    {GAME_ID_PONG, PAGE_PONG, "Pong", "DUEL", "Win the rally before AI locks in.", "UP/DN Move bat", "OK Serve"},
    {GAME_ID_SNAKE, PAGE_SNAKE, "Snake", "GRID", "Feed steady, chase blink bonus safely.", "UP Left turn  DN Right", "BK Pause  long exit"},
    {GAME_ID_TETRIS, PAGE_TETRIS, "Tetris", "STACK", "Rotate tight, recover wall jams fast.", "UP/DN Move  OK Rotate", "OK long Drop  BK Pause"},
    {GAME_ID_SHOOTER, PAGE_SHOOTER, "Shooter", "WAVE", "Clear lanes before pressure stacks up.", "UP/DN Move ship", "OK Fire"},
};

uint8_t ui_game_count(void)
{
    return (uint8_t)(sizeof(g_games) / sizeof(g_games[0]));
}

PageId ui_game_page_from_index(uint8_t index)
{
    if (index >= ui_game_count()) {
        return PAGE_BREAKOUT;
    }
    return g_games[index].page_id;
}

GameId ui_game_id_from_page(PageId page)
{
    for (uint8_t i = 0U; i < ui_game_count(); ++i) {
        if (g_games[i].page_id == page) {
            return g_games[i].game_id;
        }
    }
    return GAME_ID_BREAKOUT;
}

GameId ui_game_id_from_index(uint8_t index)
{
    if (index >= ui_game_count()) {
        return GAME_ID_BREAKOUT;
    }
    return g_games[index].game_id;
}

const char *ui_game_name(GameId game_id)
{
    for (uint8_t i = 0U; i < ui_game_count(); ++i) {
        if (g_games[i].game_id == game_id) {
            return g_games[i].name;
        }
    }
    return "Game";
}

const char *ui_game_detail_tag(GameId game_id)
{
    for (uint8_t i = 0U; i < ui_game_count(); ++i) {
        if (g_games[i].game_id == game_id) {
            return g_games[i].tag;
        }
    }
    return "PLAY";
}

const char *ui_game_detail_desc(GameId game_id)
{
    for (uint8_t i = 0U; i < ui_game_count(); ++i) {
        if (g_games[i].game_id == game_id) {
            return g_games[i].desc;
        }
    }
    return "";
}

const char *ui_game_detail_controls_a(GameId game_id)
{
    for (uint8_t i = 0U; i < ui_game_count(); ++i) {
        if (g_games[i].game_id == game_id) {
            return g_games[i].controls_a;
        }
    }
    return "";
}

const char *ui_game_detail_controls_b(GameId game_id)
{
    for (uint8_t i = 0U; i < ui_game_count(); ++i) {
        if (g_games[i].game_id == game_id) {
            return g_games[i].controls_b;
        }
    }
    return "";
}

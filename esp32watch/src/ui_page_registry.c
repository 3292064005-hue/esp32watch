#include "ui_page_registry.h"

static const UiPageOps g_page_registry[PAGE_COUNT] = {
    [PAGE_WATCHFACE]   = {.title = "WATCH", .refresh_policy = UI_REFRESH_WATCHFACE, .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_ROOT,     .presenter = UI_PRESENTER_WATCHFACE, .render = ui_page_watchface_render,   .handle = ui_page_watchface_handle},
    [PAGE_QUICK]       = {.title = "QUICK", .refresh_policy = UI_REFRESH_CARD,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_ROOT,     .presenter = UI_PRESENTER_WATCHFACE, .render = ui_page_quick_render,       .handle = ui_page_quick_handle},
    [PAGE_APPS]        = {.title = "APPS",  .refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_ROOT,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_apps_render,        .handle = ui_page_apps_handle},
    [PAGE_ALARM]       = {.title = "ALARM", .refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_alarm_render,       .handle = ui_page_alarm_handle},
    [PAGE_ALARM_EDIT]  = {.title = "ALM ED",.refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_EDIT,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_alarm_edit_render,  .handle = ui_page_alarm_edit_handle},
    [PAGE_STOPWATCH]   = {.title = "SW",    .refresh_policy = UI_REFRESH_STOPWATCH, .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_stopwatch_render,   .handle = ui_page_stopwatch_handle},
    [PAGE_TIMER]       = {.title = "TIMER", .refresh_policy = UI_REFRESH_TIMER,     .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_timer_render,       .handle = ui_page_timer_handle},
    [PAGE_MUSIC]       = {.title = "MUSIC", .refresh_policy = UI_REFRESH_CARD,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_music_render,       .handle = ui_page_music_handle},
    [PAGE_GAMES]       = {.title = "GAMES", .refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_games_menu_render,  .handle = ui_page_games_menu_handle},
    [PAGE_GAME_DETAIL] = {.title = "GAME",  .refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_game_detail_render, .handle = ui_page_game_detail_handle},
    [PAGE_BREAKOUT]    = {.title = "BRK",   .refresh_policy = UI_REFRESH_GAME,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_breakout_render,    .handle = ui_page_breakout_handle},
    [PAGE_DINO]        = {.title = "DINO",  .refresh_policy = UI_REFRESH_GAME,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_dino_render,        .handle = ui_page_dino_handle},
    [PAGE_PONG]        = {.title = "PONG",  .refresh_policy = UI_REFRESH_GAME,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_pong_render,        .handle = ui_page_pong_handle},
    [PAGE_SNAKE]       = {.title = "SNAKE", .refresh_policy = UI_REFRESH_GAME,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_snake_render,       .handle = ui_page_snake_handle},
    [PAGE_TETRIS]      = {.title = "TET",   .refresh_policy = UI_REFRESH_GAME,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_tetris_render,      .handle = ui_page_tetris_handle},
    [PAGE_SHOOTER]     = {.title = "SHOT",  .refresh_policy = UI_REFRESH_GAME,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_shooter_render,     .handle = ui_page_shooter_handle},
    [PAGE_ACTIVITY]    = {.title = "ACT",   .refresh_policy = UI_REFRESH_CARD,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_ACTIVITY,  .render = ui_page_activity_render,    .handle = ui_page_activity_handle},
    [PAGE_SENSOR]      = {.title = "SENSOR",.refresh_policy = UI_REFRESH_SENSOR,    .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_SENSOR,    .render = ui_page_sensor_render,      .handle = ui_page_sensor_handle},
    [PAGE_SETTINGS]    = {.title = "SET",   .refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_SETTINGS, .presenter = UI_PRESENTER_SETTINGS,  .render = ui_page_settings_main_render,.handle = ui_page_settings_main_handle},
    [PAGE_TIMESET]     = {.title = "TIME",  .refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = false, .allow_popup = false,.nav_group = UI_NAV_GROUP_EDIT,     .presenter = UI_PRESENTER_NONE,      .render = ui_page_time_set_render,    .handle = ui_page_time_set_handle},
    [PAGE_DIAG]        = {.title = "DIAG",  .refresh_policy = UI_REFRESH_CARD,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_SYSTEM,   .presenter = UI_PRESENTER_DIAG,      .render = ui_page_diag_render,        .handle = ui_page_diag_handle},
    [PAGE_ABOUT]       = {.title = "ABOUT", .refresh_policy = UI_REFRESH_NONE,      .allow_auto_sleep = true,  .allow_popup = true, .nav_group = UI_NAV_GROUP_SYSTEM,   .presenter = UI_PRESENTER_SYSTEM,    .render = ui_page_about_render,       .handle = ui_page_about_handle},
    [PAGE_CALIBRATION] = {.title = "CAL",   .refresh_policy = UI_REFRESH_CARD,      .allow_auto_sleep = false, .allow_popup = false,.nav_group = UI_NAV_GROUP_SYSTEM,   .presenter = UI_PRESENTER_SENSOR,    .render = ui_page_calibration_render, .handle = ui_page_calibration_handle},
    [PAGE_INPUTTEST]   = {.title = "INPUT", .refresh_policy = UI_REFRESH_CARD,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_SYSTEM,   .presenter = UI_PRESENTER_SYSTEM,    .render = ui_page_inputtest_render,   .handle = ui_page_inputtest_handle},
    [PAGE_STORAGE]     = {.title = "STORE", .refresh_policy = UI_REFRESH_CARD,      .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_SYSTEM,   .presenter = UI_PRESENTER_STORAGE,   .render = ui_page_storage_render,     .handle = ui_page_storage_handle},
    [PAGE_LIQUID]      = {.title = "LIQ",   .refresh_policy = UI_REFRESH_LIQUID,    .allow_auto_sleep = false, .allow_popup = true, .nav_group = UI_NAV_GROUP_APPS,     .presenter = UI_PRESENTER_SENSOR,    .render = ui_page_liquid_render,      .handle = ui_page_liquid_handle},
};

static const UiPageOps g_null_page = {
    .title = "N/A",
    .refresh_policy = UI_REFRESH_NONE,
    .allow_auto_sleep = true,
    .allow_popup = true,
    .nav_group = UI_NAV_GROUP_ROOT,
    .presenter = UI_PRESENTER_NONE,
    .render = 0,
    .handle = 0,
};

const UiPageOps *ui_page_registry_get(PageId page)
{
    if (page >= PAGE_COUNT) {
        return &g_null_page;
    }
    if (g_page_registry[page].render == 0 && g_page_registry[page].handle == 0) {
        return &g_null_page;
    }
    return &g_page_registry[page];
}

const char *ui_page_registry_title(PageId page)
{
    return ui_page_registry_get(page)->title;
}

UiPageRefreshPolicy ui_page_registry_refresh_policy(PageId page)
{
    return ui_page_registry_get(page)->refresh_policy;
}

bool ui_page_registry_allows_auto_sleep(PageId page)
{
    return ui_page_registry_get(page)->allow_auto_sleep;
}

bool ui_page_registry_allows_popup(PageId page)
{
    return ui_page_registry_get(page)->allow_popup;
}

uint8_t ui_page_registry_nav_group(PageId page)
{
    return ui_page_registry_get(page)->nav_group;
}


UiPagePresenterId ui_page_registry_presenter(PageId page)
{
    return (UiPagePresenterId)ui_page_registry_get(page)->presenter;
}

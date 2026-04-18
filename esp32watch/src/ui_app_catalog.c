#include "ui_app_catalog.h"

static const UiAppDescriptor g_ui_app_catalog[] = {
    {.name = "Alarms",    .page = PAGE_ALARM,       .accent = false, .compose_status = ui_status_compose_alarm_value},
    {.name = "Timer",     .page = PAGE_TIMER,       .accent = false, .compose_status = NULL},
    {.name = "Melody",    .page = PAGE_MUSIC,       .accent = false, .compose_status = ui_status_compose_music_value},
    {.name = "Activity",  .page = PAGE_ACTIVITY,    .accent = false, .compose_status = ui_status_compose_activity_value},
    {.name = "Settings",  .page = PAGE_SETTINGS,    .accent = false, .compose_status = NULL},
    {.name = "Games",     .page = PAGE_GAMES,       .accent = false, .compose_status = NULL},
    {.name = "Stopwatch", .page = PAGE_STOPWATCH,   .accent = true,  .compose_status = NULL},
    {.name = "Sensor",    .page = PAGE_SENSOR,      .accent = true,  .compose_status = ui_status_compose_sensor_value},
    {.name = "Diag",      .page = PAGE_DIAG,        .accent = true,  .compose_status = ui_status_compose_diag_value},
    {.name = "Storage",   .page = PAGE_STORAGE,     .accent = true,  .compose_status = ui_status_compose_storage_value},
    {.name = "Calib",     .page = PAGE_CALIBRATION, .accent = true,  .compose_status = NULL},
    {.name = "Input",     .page = PAGE_INPUTTEST,   .accent = true,  .compose_status = NULL},
    {.name = "Liquid",    .page = PAGE_LIQUID,      .accent = true,  .compose_status = NULL},
    {.name = "About",     .page = PAGE_ABOUT,       .accent = true,  .compose_status = NULL},
};

uint8_t ui_app_catalog_count(void)
{
    return (uint8_t)(sizeof(g_ui_app_catalog) / sizeof(g_ui_app_catalog[0]));
}

bool ui_app_catalog_get(uint8_t index, UiAppDescriptor *out)
{
    if (out == NULL || index >= ui_app_catalog_count()) {
        return false;
    }

    *out = g_ui_app_catalog[index];
    return true;
}

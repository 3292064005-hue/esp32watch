#include "ui_app_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void ui_status_compose_alarm_value(char *out, size_t out_size)
{
    if (out != NULL && out_size > 0U) {
        snprintf(out, out_size, "ALARM");
    }
}

void ui_status_compose_music_value(char *out, size_t out_size)
{
    if (out != NULL && out_size > 0U) {
        snprintf(out, out_size, "MUSIC");
    }
}

void ui_status_compose_activity_value(char *out, size_t out_size)
{
    if (out != NULL && out_size > 0U) {
        snprintf(out, out_size, "ACT");
    }
}

void ui_status_compose_sensor_value(char *out, size_t out_size)
{
    if (out != NULL && out_size > 0U) {
        snprintf(out, out_size, "SENSOR");
    }
}

void ui_status_compose_diag_value(char *out, size_t out_size)
{
    if (out != NULL && out_size > 0U) {
        snprintf(out, out_size, "DIAG");
    }
}

void ui_status_compose_storage_value(char *out, size_t out_size)
{
    if (out != NULL && out_size > 0U) {
        snprintf(out, out_size, "STORE");
    }
}

int main(void)
{
    UiAppDescriptor app = {0};

    assert(ui_app_catalog_count() >= 10U);
    assert(!ui_app_catalog_get(255U, &app));
    assert(!ui_app_catalog_get(0U, NULL));

    assert(ui_app_catalog_get(0U, &app));
    assert(strcmp(app.name, "Alarms") == 0);
    assert(app.page == PAGE_ALARM);
    assert(app.compose_status == ui_status_compose_alarm_value);

    assert(ui_app_catalog_get(6U, &app));
    assert(strcmp(app.name, "Stopwatch") == 0);
    assert(app.page == PAGE_STOPWATCH);
    assert(app.accent);
    assert(app.compose_status == NULL);

    assert(ui_app_catalog_get(ui_app_catalog_count() - 1U, &app));
    assert(strcmp(app.name, "About") == 0);
    assert(app.page == PAGE_ABOUT);

    puts("[OK] ui_app_catalog manifest behavior check passed");
    return 0;
}

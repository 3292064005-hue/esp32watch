#include "ui_page_catalog.h"
#include "ui_page_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DEFINE_PAGE_STUB(name) \
    void name##_render(PageId page, int16_t ox) { (void)page; (void)ox; } \
    bool name##_handle(PageId page, const KeyEvent *e, uint32_t now_ms) { (void)page; (void)e; (void)now_ms; return false; }

DEFINE_PAGE_STUB(ui_page_watchface)
DEFINE_PAGE_STUB(ui_page_quick)
DEFINE_PAGE_STUB(ui_page_apps)
DEFINE_PAGE_STUB(ui_page_alarm)
DEFINE_PAGE_STUB(ui_page_alarm_edit)
DEFINE_PAGE_STUB(ui_page_stopwatch)
DEFINE_PAGE_STUB(ui_page_timer)
DEFINE_PAGE_STUB(ui_page_music)
DEFINE_PAGE_STUB(ui_page_games_menu)
DEFINE_PAGE_STUB(ui_page_game_detail)
DEFINE_PAGE_STUB(ui_page_breakout)
DEFINE_PAGE_STUB(ui_page_dino)
DEFINE_PAGE_STUB(ui_page_pong)
DEFINE_PAGE_STUB(ui_page_snake)
DEFINE_PAGE_STUB(ui_page_tetris)
DEFINE_PAGE_STUB(ui_page_shooter)
DEFINE_PAGE_STUB(ui_page_activity)
DEFINE_PAGE_STUB(ui_page_sensor)
DEFINE_PAGE_STUB(ui_page_settings_main)
DEFINE_PAGE_STUB(ui_page_time_set)
DEFINE_PAGE_STUB(ui_page_diag)
DEFINE_PAGE_STUB(ui_page_about)
DEFINE_PAGE_STUB(ui_page_calibration)
DEFINE_PAGE_STUB(ui_page_inputtest)
DEFINE_PAGE_STUB(ui_page_storage)
DEFINE_PAGE_STUB(ui_page_liquid)

int main(void)
{
    UiPageDescriptor descriptor = {0};
    const UiPageOps *ops = NULL;

    assert(ui_page_catalog_count() == PAGE_COUNT);
    assert(!ui_page_catalog_get(PAGE_COUNT, &descriptor));
    assert(!ui_page_catalog_get(0U, NULL));
    assert(ui_page_catalog_get(0U, &descriptor));
    assert(descriptor.page == PAGE_WATCHFACE);
    assert(strcmp(descriptor.ops.title, "WATCH") == 0);

    assert(ui_page_catalog_find(PAGE_STORAGE) != NULL);
    assert(ui_page_catalog_find(PAGE_COUNT) == NULL);

    ops = ui_page_registry_get(PAGE_STORAGE);
    assert(strcmp(ops->title, "STORE") == 0);
    assert(ui_page_registry_presenter(PAGE_SENSOR) == UI_PRESENTER_SENSOR);
    assert(ui_page_registry_nav_group(PAGE_SETTINGS) == UI_NAV_GROUP_SETTINGS);
    assert(!ui_page_registry_allows_auto_sleep(PAGE_TIMER));
    assert(ui_page_registry_allows_popup(PAGE_DIAG));

    ops = ui_page_registry_get(PAGE_COUNT);
    assert(strcmp(ops->title, "N/A") == 0);

    puts("[OK] ui_page_catalog manifest behavior check passed");
    return 0;
}

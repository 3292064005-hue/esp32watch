#include <stddef.h>
#include "ui_internal.h"
#include "ui_page_registry.h"
#include "model.h"
#include "melody_service.h"

static void ui_core_start_anim(PageId next, int8_t dir, uint32_t now_ms)
{
    ModelDomainState domain_state;

    if (next == ui_runtime_get_current_page()) return;
    if (ui_runtime_get_current_page() == PAGE_MUSIC && next != PAGE_MUSIC) {
        melody_stop();
    }
    ui_runtime_set_from_page(ui_runtime_get_current_page());
    ui_runtime_set_to_page(next);
    if (ui_page_game_is_page(next)) {
        ui_page_game_reset(next);
    }
    ui_runtime_set_nav_dir(dir);
    ui_runtime_set_anim_start_ms(now_ms);
    ui_runtime_set_animating(model_get_domain_state(&domain_state) != NULL ? domain_state.settings.animations : false);
    if (!ui_runtime_is_animating()) {
        ui_runtime_set_current_page(next);
    }
    ui_core_mark_dirty();
}

void ui_core_go(PageId next, int8_t dir, uint32_t now_ms)
{
    ui_core_haptic_soft();
    ui_core_start_anim(next, dir, now_ms);
}

void ui_core_render_page(PageId page, int16_t ox)
{
    const UiPageOps *ops = ui_page_registry_get(page);
    if (ops->render != 0) {
        ops->render(page, ox);
    }
}

bool ui_core_handle_page_event(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    const UiPageOps *ops = ui_page_registry_get(page);
    if (ops->handle == 0) {
        return false;
    }
    return ops->handle(page, e, now_ms);
}

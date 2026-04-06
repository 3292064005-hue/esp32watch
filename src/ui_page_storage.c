#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_storage_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];
    (void)page;

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Storage");
    snprintf(line, sizeof(line), "backend %s v%u", snap.storage.backend_name, snap.storage.version);
    display_draw_text_5x7(ox + 8, 12, line, true);

    snprintf(line, sizeof(line), "init %s crc %s", snap.storage.initialized ? "ok" : "no",
             snap.storage.crc_valid ? "ok" : "bad");
    display_draw_text_5x7(ox + 8, 21, line, true);

    snprintf(line, sizeof(line), "pend %02X dirty %02X", (unsigned)snap.storage.pending_mask,
             (unsigned)snap.storage.dirty_source_mask);
    display_draw_text_5x7(ox + 8, 30, line, true);

    snprintf(line, sizeof(line), "stored %04X calc %04X", snap.storage.stored_crc,
             snap.storage.calculated_crc);
    display_draw_text_5x7(ox + 8, 39, line, true);

    snprintf(line, sizeof(line), "c%lu %s %s", (unsigned long)snap.storage.commit_count,
             snap.storage.last_commit_ok ? "ok" : "bad",
             snap.storage.last_commit_reason_name);
    display_draw_text_5x7(ox + 8, 48, line, true);

    snprintf(line, sizeof(line), "%s %s wear %lu", snap.storage.commit_state_name,
             snap.storage.sleep_flush_pending ? "flush" : "idle",
             (unsigned long)snap.storage.wear_count);
    display_draw_text_5x7(ox + 8, 57, line, true);
}

bool ui_page_storage_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_OK) {
        ui_request_storage_manual_flush();
        return true;
    }
    if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }
    return false;
}

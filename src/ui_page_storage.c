#include "ui_internal.h"
#include "display.h"
#include <stdio.h>

void ui_page_storage_render(PageId page, int16_t ox)
{
    UiSystemSnapshot snap;
    char line[24];

    (void)page;
    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Storage");
    ui_core_draw_card(ox + 8, 14, 112, 22, snap.storage.backend_name);
    snprintf(line, sizeof(line), "V%u  %s", snap.storage.version, snap.storage.crc_valid ? "CRC OK" : "CRC BAD");
    display_draw_text_centered_5x7(ox, 22, 128, line, true);
    snprintf(line, sizeof(line), "%02X / %02X", (unsigned)snap.storage.pending_mask, (unsigned)snap.storage.dirty_source_mask);
    ui_core_draw_kv_row(ox + 10, 39, 108, "PEND", line);
    snprintf(line, sizeof(line), "%lu", (unsigned long)snap.storage.commit_count);
    snprintf(line, sizeof(line), "%lu / %lu", (unsigned long)snap.storage.commit_count, (unsigned long)snap.storage.wear_count);
    ui_core_draw_kv_row(ox + 10, 47, 108, "CNT/WEAR", line);
    ui_core_draw_footer_hint(ox, "OK Flush  BK Back");
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

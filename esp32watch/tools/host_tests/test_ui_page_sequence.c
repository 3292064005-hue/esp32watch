#include "ui_internal.h"
#include "display_internal.h"
#include "model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ModelDomainState g_domain;
static ModelRuntimeState g_runtime;
static UiSystemSnapshot g_snap;

bool display_backend_init(void) { return true; }
void display_backend_set_contrast(uint8_t value) { (void)value; }
void display_backend_set_sleep(bool sleep) { (void)sleep; }
bool display_backend_flush(const uint8_t *buffer) { (void)buffer; return true; }
void diag_service_note_display_tx_fail(uint16_t code) { (void)code; }

const ModelDomainState *model_get_domain_state(ModelDomainState *out) { if (!out) return NULL; *out = g_domain; return out; }
const ModelRuntimeState *model_get_runtime_state(ModelRuntimeState *out) { if (!out) return NULL; *out = g_runtime; return out; }
const ModelUiState *model_get_ui_state(ModelUiState *out) { static ModelUiState s; if (!out) return NULL; *out = s; return out; }
const char *ui_weekday_name(uint8_t weekday) {
    static const char *names[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    return names[weekday % 7U];
}
void ui_get_system_snapshot(UiSystemSnapshot *out) { if (out) *out = g_snap; }
void ui_status_compose_header_tags(char *out, size_t out_size) { if (out && out_size) { strncpy(out, "SYNC", out_size - 1U); out[out_size - 1U] = '\0'; } }
void ui_core_draw_status_bar(int16_t ox) { display_draw_rect(ox, 0, 128, 10, true); }
void ui_core_draw_header(int16_t ox, const char *title) { display_draw_rect(ox, 0, 128, 12, true); display_draw_text_centered_5x7(ox, 2, 128, title, true); }
void ui_core_draw_card(int16_t x, int16_t y, int16_t w, int16_t h, const char *title) { display_draw_round_rect(x, y, w, h, true); display_draw_text_centered_5x7(x, y + 2, w, title, true); }
void ui_core_draw_kv_row(int16_t x, int16_t y, int16_t w, const char *key, const char *value) { display_draw_text_5x7(x, y, key, true); display_draw_text_right_5x7(x + w, y, value, true); }
void ui_core_draw_footer_hint(int16_t ox, const char *hint) { display_draw_hline(ox, 55, 128, true); display_draw_text_centered_5x7(ox, 57, 128, hint, true); }
void ui_request_storage_manual_flush(void) {}
void ui_request_cycle_watchface(int8_t dir) { (void)dir; }
void ui_core_go(PageId page, int8_t dir, uint32_t now_ms) { (void)page; (void)dir; (void)now_ms; }
void ui_request_sensor_reinit(void) {}
void ui_request_sensor_calibration(void) {}
void ui_request_clear_safe_mode(void) {}
bool web_wifi_connected(void) { return true; }
bool diag_service_is_safe_mode_active(void) { return false; }

static uint32_t fnv1a32(const uint8_t *data, size_t len)
{
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 16777619UL;
    }
    return h;
}

static void seed_state(void)
{
    memset(&g_domain, 0, sizeof(g_domain));
    memset(&g_runtime, 0, sizeof(g_runtime));
    memset(&g_snap, 0, sizeof(g_snap));

    g_domain.time_valid = true;
    g_domain.now.year = 2026; g_domain.now.month = 4; g_domain.now.day = 13; g_domain.now.weekday = 1;
    g_domain.now.hour = 9; g_domain.now.minute = 41; g_domain.now.second = 7;
    g_domain.settings.watchface = 0U;
    g_domain.settings.show_seconds = true;
    g_domain.activity.goal = 8000U;
    g_domain.activity.steps = 3456U;
    g_runtime.sensor.online = true;
    g_runtime.sensor.quality = 5U;
    g_snap.activity.steps = 3456U;
    g_snap.activity.goal = 8000U;
    g_snap.activity.goal_percent = 43U;
    g_snap.activity.active_minutes = 27U;
    g_snap.storage.version = 6U;
    g_snap.storage.crc_valid = true;
    g_snap.storage.pending_mask = 0x01U;
    g_snap.storage.dirty_source_mask = 0x03U;
    g_snap.storage.commit_count = 7U;
    g_snap.storage.wear_count = 9U;
    g_snap.storage.backend_name = "RTC_RESET_DOMAIN";
}

static uint32_t render_crc(PageId page)
{
    display_clear();
    switch (page) {
        case PAGE_WATCHFACE:
            ui_page_watchface_render(page, 0);
            break;
        case PAGE_ACTIVITY:
            ui_page_activity_render(page, 0);
            break;
        case PAGE_STORAGE:
            ui_page_storage_render(page, 0);
            break;
        case PAGE_ABOUT:
            ui_page_about_render(page, 0);
            break;
        default:
            assert(!"unexpected page in sequence test");
    }
    return fnv1a32(g_oled_buffer, sizeof(g_oled_buffer));
}

int main(void)
{
    seed_state();
    display_init();

    const PageId sequence[] = {PAGE_WATCHFACE, PAGE_ACTIVITY, PAGE_STORAGE, PAGE_ABOUT};
    uint32_t aggregate = 2166136261UL;
    for (size_t i = 0; i < (sizeof(sequence) / sizeof(sequence[0])); ++i) {
        uint32_t crc = render_crc(sequence[i]);
        aggregate ^= crc & 0xFFU; aggregate *= 16777619UL;
        aggregate ^= (crc >> 8) & 0xFFU; aggregate *= 16777619UL;
        aggregate ^= (crc >> 16) & 0xFFU; aggregate *= 16777619UL;
        aggregate ^= (crc >> 24) & 0xFFU; aggregate *= 16777619UL;
    }

    assert(aggregate == 0xB367BF09UL);
    puts("[OK] ui page sequence replay check passed");
    return 0;
}

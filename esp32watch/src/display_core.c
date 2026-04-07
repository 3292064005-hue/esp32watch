#include "display_internal.h"
#include "services/diag_service.h"
#include <string.h>

uint8_t g_oled_buffer[OLED_BUFFER_SIZE];
uint8_t g_prev_buffer[OLED_BUFFER_SIZE];

static uint32_t g_present_count;
static uint32_t g_full_refresh_count;
static uint32_t g_tx_error_count;

void display_core_note_full_refresh(void)
{
    g_full_refresh_count++;
}

void display_core_note_tx_error(PlatformStatus status)
{
    g_tx_error_count++;
    diag_service_note_display_tx_fail((uint16_t)status);
}

PlatformStatus display_core_write_cmds(const uint8_t *cmds, uint8_t count)
{
    (void)cmds;
    (void)count;
    return PLATFORM_STATUS_OK;
}

PlatformStatus display_core_write_data_chunk(const uint8_t *data, uint8_t len)
{
    (void)data;
    (void)len;
    return PLATFORM_STATUS_OK;
}

void display_core_set_window(uint8_t page, uint8_t col_start, uint8_t col_end)
{
    uint8_t cmds[] = {0x21, col_start, col_end, 0x22, page, page};
    (void)display_core_write_cmds(cmds, sizeof(cmds));
}

void display_invalidate_cache(void)
{
    memset(g_prev_buffer, 0xFF, sizeof(g_prev_buffer));
}

void display_init(void)
{
    (void)display_backend_init();
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
    memset(g_prev_buffer, 0, sizeof(g_prev_buffer));
    g_present_count = 0U;
    g_full_refresh_count = 0U;
    g_tx_error_count = 0U;
    display_invalidate_cache();
    display_core_note_full_refresh();
    display_clear();
    (void)display_present();
}

void display_set_contrast(uint8_t value)
{
    display_backend_set_contrast(value);
}

void display_sleep(bool sleep)
{
    display_backend_set_sleep(sleep);
}

void display_clear(void)
{
    memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
}

bool display_present(void)
{
    bool ok = false;
    g_present_count++;
    ok = display_backend_flush(g_oled_buffer);
    if (ok) {
        memcpy(g_prev_buffer, g_oled_buffer, sizeof(g_prev_buffer));
    } else {
        display_core_note_tx_error(PLATFORM_STATUS_ERROR);
        display_invalidate_cache();
    }
    return ok;
}

uint32_t display_get_present_count(void)
{
    return g_present_count;
}

uint32_t display_get_full_refresh_count(void)
{
    return g_full_refresh_count;
}

uint32_t display_get_tx_error_count(void)
{
    return g_tx_error_count;
}


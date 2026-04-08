#include "ui_internal.h"
#include "app_config.h"
#include "display.h"
#include <stdio.h>

static void draw_activity(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Activity");
    snprintf(line, sizeof(line), "steps %lu", (unsigned long)snap.activity.steps);
    display_draw_text_centered_5x7(ox, 18, 128, line, true);
    snprintf(line, sizeof(line), "goal %lu  %u%%", (unsigned long)snap.activity.goal,
             snap.activity.goal_percent);
    display_draw_text_centered_5x7(ox, 29, 128, line, true);
    display_draw_progress_bar(ox + 16, 40, 96, 10, snap.activity.goal_percent, false);
    snprintf(line, sizeof(line), "lvl %u state %u active %u", snap.activity.activity_level,
             snap.activity.motion_state, snap.activity.active_minutes);
    display_draw_text_centered_5x7(ox, 55, 128, line, true);
}

static void draw_sensor(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];
    char value[20];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Sensor");
    ui_core_draw_card(ox + 8, 14, 112, 16, snap.sensor.online ? "ONLINE" : "OFFLINE");
    snprintf(value, sizeof(value), "Q%u  CAL %u%%", snap.sensor.quality, snap.sensor.calibration_progress);
    display_draw_text_5x7(ox + 14, 21, value, true);
    display_draw_text_right_5x7(ox + 112, 21, snap.sensor.runtime_state_name, true);
    ui_core_draw_card(ox + 8, 32, 112, 20, "MOTION");
    snprintf(line, sizeof(line), "P %d  R %d  M %d", snap.sensor.pitch_deg, snap.sensor.roll_deg, snap.sensor.motion_score);
    display_draw_text_5x7(ox + 14, 40, line, true);
    snprintf(line, sizeof(line), "A %d %d  G %d", snap.sensor.ax, snap.sensor.ay, snap.sensor.gz);
    display_draw_text_5x7(ox + 14, 48, line, true);
    ui_core_draw_footer_hint(ox, "OK Reinit  LONG Cal");
}

static void draw_diag(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Diag");
    ui_core_draw_card(ox + 8, 14, 112, 14, snap.safe_mode_active ? "SAFE MODE" : "SYSTEM OK");
    snprintf(line, sizeof(line), "%s  %s", snap.reset_reason_name, snap.wake_reason_name);
    display_draw_text_5x7(ox + 14, 20, line, true);
    ui_core_draw_card(ox + 8, 30, 112, 14, "FAULT");
    snprintf(line, sizeof(line), "%s  %s", snap.has_last_fault ? snap.last_fault_name : "NONE",
             snap.has_last_fault ? snap.last_fault_severity_name : "IDLE");
    display_draw_text_5x7(ox + 14, 36, line, true);
    ui_core_draw_card(ox + 8, 46, 112, 14, "COUNTS");
    snprintf(line, sizeof(line), "IMU %u/%u UI %lu DSP %lu",
             snap.sensor.reinit_count, snap.sensor.fault_count,
             (unsigned long)snap.ui_mutation_overflow_event_count,
             (unsigned long)snap.display_tx_fail_count);
    display_draw_text_5x7(ox + 14, 52, line, true);
    ui_core_draw_footer_hint(ox, snap.safe_mode_active ? "OK Clear Safe  BK Locked" : "BK Back");
}

static void draw_calibration(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[28];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Calibration");
    snprintf(line, sizeof(line), "imu %s", snap.sensor.runtime_state_name);
    display_draw_text_centered_5x7(ox, 17, 128, line, true);
    snprintf(line, sizeof(line), "static %s  q%u", snap.sensor.static_now ? "yes" : "no", snap.sensor.quality);
    display_draw_text_centered_5x7(ox, 28, 128, line, true);
    snprintf(line, sizeof(line), "progress %u%%", snap.sensor.calibration_progress);
    display_draw_text_centered_5x7(ox, 39, 128, line, true);
    display_draw_progress_bar(ox + 18, 49, 92, 8, snap.sensor.calibration_progress, false);
    ui_core_draw_footer_hint(ox, "OK Start  BK Back");
}

static void draw_inputtest(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[24];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Input Test");
    ui_core_draw_card(ox + 8, 14, 112, 36, "KEY STATE");
    snprintf(line, sizeof(line), "UP    %s", snap.key_up_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 20, line, true);
    snprintf(line, sizeof(line), "DOWN  %s", snap.key_down_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 28, line, true);
    snprintf(line, sizeof(line), "OK    %s", snap.key_ok_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 36, line, true);
    snprintf(line, sizeof(line), "BACK  %s", snap.key_back_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 44, line, true);
    ui_core_draw_footer_hint(ox, "BK Back");
}

static void draw_storage(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Storage");
    ui_core_draw_card(ox + 8, 14, 112, 14, snap.storage.crc_valid ? "STORAGE OK" : "STORAGE WARN");
    snprintf(line, sizeof(line), "%s  V%u  %s", snap.storage.backend_name, snap.storage.version, snap.storage.commit_state_name);
    display_draw_text_5x7(ox + 14, 20, line, true);
    ui_core_draw_card(ox + 8, 30, 112, 14, "CRC");
    snprintf(line, sizeof(line), "S %04X  C %04X  D%02X", snap.storage.stored_crc, snap.storage.calculated_crc, (unsigned)snap.storage.dirty_source_mask);
    display_draw_text_5x7(ox + 14, 36, line, true);
    ui_core_draw_card(ox + 8, 46, 112, 14, "COMMIT");
    snprintf(line, sizeof(line), "C%lu  %s  %s", (unsigned long)snap.storage.commit_count,
             snap.storage.last_commit_ok ? "OK" : "BAD",
             snap.storage.sleep_flush_pending ? "FLUSH" : "IDLE");
    display_draw_text_5x7(ox + 14, 52, line, true);
    ui_core_draw_footer_hint(ox, "OK Flush  BK Back");
}

static void draw_about(int16_t ox)
{
    ui_core_draw_header(ox, "About");
    ui_core_draw_card(ox + 8, 14, 112, 16, "DEVICE");
    display_draw_text_5x7(ox + 14, 21, "ESP32 watch terminal", true);
    ui_core_draw_card(ox + 8, 32, 112, 20, "STACK");
    display_draw_text_5x7(ox + 14, 40, "RTC/OLED/MPU6050/Web", true);
    display_draw_text_5x7(ox + 14, 48, "Diag + Storage + Input", true);
    ui_core_draw_footer_hint(ox, "BK Back");
}

void ui_page_system_render(PageId page, int16_t ox)
{
    switch (page) {
        case PAGE_ACTIVITY: draw_activity(ox); break;
        case PAGE_SENSOR: draw_sensor(ox); break;
        case PAGE_DIAG: draw_diag(ox); break;
        case PAGE_ABOUT: draw_about(ox); break;
        case PAGE_CALIBRATION: draw_calibration(ox); break;
        case PAGE_INPUTTEST: draw_inputtest(ox); break;
        case PAGE_STORAGE: draw_storage(ox); break;
        default: break;
    }
}

bool ui_page_system_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    if (page == PAGE_SENSOR) {
        if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_OK) {
            ui_request_sensor_reinit();
            return true;
        }
        if (e->type == KEY_EVENT_LONG && e->id == KEY_ID_OK) {
            ui_request_sensor_calibration();
            return true;
        }
        if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
            ui_core_go(PAGE_APPS, -1, now_ms);
            return true;
        }
        return false;
    }

    if (page == PAGE_CALIBRATION) {
        if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_LONG) && e->id == KEY_ID_OK) {
            ui_request_sensor_calibration();
            return true;
        }
        if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
            ui_core_go(PAGE_APPS, -1, now_ms);
            return true;
        }
        return false;
    }

    if (page == PAGE_DIAG) {
        UiSystemSnapshot snap;
        ui_get_system_snapshot(&snap);
        if ((e->type == KEY_EVENT_SHORT || e->type == KEY_EVENT_LONG) && e->id == KEY_ID_OK && snap.safe_mode_active) {
            ui_request_clear_safe_mode();
            if (snap.safe_mode_can_clear) {
                ui_core_go(PAGE_WATCHFACE, 1, now_ms);
            }
            return true;
        }
        if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
            if (snap.safe_mode_active) {
                return true;
            }
            ui_core_go(PAGE_APPS, -1, now_ms);
            return true;
        }
        return false;
    }

    if (page == PAGE_STORAGE) {
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

    if (page == PAGE_ACTIVITY || page == PAGE_ABOUT || page == PAGE_INPUTTEST) {
        if (e->type == KEY_EVENT_SHORT && e->id == KEY_ID_BACK) {
            ui_core_go(PAGE_APPS, -1, now_ms);
            return true;
        }
    }
    return false;
}

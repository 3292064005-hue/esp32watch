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

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Sensor");
    snprintf(line, sizeof(line), "%s q%u e%u", snap.sensor.runtime_state_name,
             snap.sensor.quality, snap.sensor.error_code);
    display_draw_text_5x7(ox + 6, 16, line, true);
    snprintf(line, sizeof(line), "A %d %d %d", snap.sensor.ax, snap.sensor.ay, snap.sensor.az);
    display_draw_text_5x7(ox + 6, 25, line, true);
    snprintf(line, sizeof(line), "G %d %d %d", snap.sensor.gx, snap.sensor.gy, snap.sensor.gz);
    display_draw_text_5x7(ox + 6, 34, line, true);
    snprintf(line, sizeof(line), "P %d R %d M %d", snap.sensor.pitch_deg,
             snap.sensor.roll_deg, snap.sensor.motion_score);
    display_draw_text_5x7(ox + 6, 43, line, true);
    snprintf(line, sizeof(line), "%s %u%% bk%lus", snap.sensor.calibration_state_name,
             snap.sensor.calibration_progress,
             (unsigned long)snap.sensor.retry_backoff_s);
    display_draw_text_5x7(ox + 6, 52, line, true);
    display_draw_text_centered_5x7(ox, 60, 128, "OK reinit LONG OK calib", true);
}

static void draw_diag(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Diagnostics");
    snprintf(line, sizeof(line), "rst %s wake %s", snap.reset_reason_name, snap.wake_reason_name);
    display_draw_text_5x7(ox + 4, 14, line, true);
    snprintf(line, sizeof(line), "slp %s stor %s", snap.sleep_reason_name,
             snap.storage.crc_valid ? "ok" : "bad");
    display_draw_text_5x7(ox + 4, 23, line, true);
    snprintf(line, sizeof(line), "imu %s rq%u fq%u", snap.sensor.runtime_state_name,
             snap.sensor.reinit_count, snap.sensor.fault_count);
    display_draw_text_5x7(ox + 4, 32, line, true);
    if (snap.sensor.online) {
        snprintf(line, sizeof(line), "qual %u mot %d", snap.sensor.quality, snap.sensor.motion_score);
    } else {
        snprintf(line, sizeof(line), "off %lu bk %lu",
                 (unsigned long)snap.sensor.offline_elapsed_s,
                 (unsigned long)snap.sensor.retry_backoff_s);
    }
    display_draw_text_5x7(ox + 4, 41, line, true);
    snprintf(line, sizeof(line), "key %u uiq %lu", (unsigned)snap.runtime.input_queue_overflow_count,
             (unsigned long)snap.ui_mutation_overflow_event_count);
    display_draw_text_5x7(ox + 4, 50, line, true);
    snprintf(line, sizeof(line), "last %s @%lus", snap.storage.last_commit_ok ? "ok" : "bad",
             (unsigned long)(snap.runtime.storage_last_commit_ms / 1000UL));
    display_draw_text_5x7(ox + 4, 59, line, true);
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
    display_draw_text_centered_5x7(ox, 59, 128, "OK start/retry  BK back", true);
}

static void draw_inputtest(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[20];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Input Test");
    snprintf(line, sizeof(line), "UP   %s", snap.key_up_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 16, line, true);
    snprintf(line, sizeof(line), "DOWN %s", snap.key_down_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 27, line, true);
    snprintf(line, sizeof(line), "OK   %s", snap.key_ok_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 38, line, true);
    snprintf(line, sizeof(line), "BACK %s", snap.key_back_down ? "DOWN" : "UP");
    display_draw_text_5x7(ox + 18, 49, line, true);
    display_draw_text_centered_5x7(ox, 60, 128, "press keys  BK back", true);
}

static void draw_storage(int16_t ox)
{
    UiSystemSnapshot snap;
    char line[32];

    ui_get_system_snapshot(&snap);
    ui_core_draw_header(ox, "Storage");
    snprintf(line, sizeof(line), "backend %s v%u", snap.storage.backend_name, snap.storage.version);
    display_draw_text_5x7(ox + 8, 15, line, true);
    snprintf(line, sizeof(line), "init %s crc %s", snap.storage.initialized ? "ok" : "no",
             snap.storage.crc_valid ? "ok" : "bad");
    display_draw_text_5x7(ox + 8, 24, line, true);
    snprintf(line, sizeof(line), "pend 0x%02X dirty 0x%02X", (unsigned)snap.storage.pending_mask,
             (unsigned)snap.storage.dirty_source_mask);
    display_draw_text_5x7(ox + 8, 33, line, true);
    snprintf(line, sizeof(line), "stored %04X calc %04X", snap.storage.stored_crc,
             snap.storage.calculated_crc);
    display_draw_text_5x7(ox + 8, 42, line, true);
    snprintf(line, sizeof(line), "c%lu %s %s", (unsigned long)snap.storage.commit_count,
             snap.storage.last_commit_ok ? "ok" : "bad",
             snap.storage_last_commit_reason_name);
    display_draw_text_5x7(ox + 8, 51, line, true);
    snprintf(line, sizeof(line), "%s %s", snap.storage.commit_state_name,
             snap.storage.sleep_flush_pending ? "flush" : "idle");
    display_draw_text_centered_5x7(ox, 60, 128, line, true);
}

static void draw_about(int16_t ox)
{
    ui_core_draw_header(ox, "About");
    display_draw_text_5x7(ox + 10, 16, "F103 watch terminal", true);
    display_draw_text_5x7(ox + 10, 26, "split UI + services", true);
    display_draw_text_5x7(ox + 10, 36, "RTC/OLED/MPU6050 stack", true);
    display_draw_text_5x7(ox + 10, 46, "diag/cal/storage tools", true);
    display_draw_text_centered_5x7(ox, 58, 128, "BACK return", true);
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

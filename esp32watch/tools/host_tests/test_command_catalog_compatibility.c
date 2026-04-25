#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_command.h"
#include "services/reset_service.h"
#include "services/sensor_service.h"
#include "services/storage_service.h"
#include "services/diag_service.h"

void model_set_brightness(uint8_t level) { (void)level; }
void model_set_goal(uint32_t goal) { (void)goal; }
void model_set_watchface(uint8_t face) { (void)face; }
void model_cycle_watchface(int8_t dir) { (void)dir; }
void model_set_screen_timeout_idx(uint8_t idx) { (void)idx; }
void model_cycle_screen_timeout(int8_t dir) { (void)dir; }
void model_set_sensor_sensitivity(uint8_t level) { (void)level; }
void model_set_auto_wake(bool enabled) { (void)enabled; }
void model_set_auto_sleep(bool enabled) { (void)enabled; }
void model_set_dnd(bool enabled) { (void)enabled; }
void model_set_vibrate(bool enabled) { (void)enabled; }
void model_set_show_seconds(bool enabled) { (void)enabled; }
void model_set_animations(bool enabled) { (void)enabled; }
void model_select_alarm_offset(int8_t delta) { (void)delta; }
void model_set_alarm_enabled_at(uint8_t index, bool enabled) { (void)index; (void)enabled; }
void model_set_alarm_time_at(uint8_t index, uint8_t hour, uint8_t minute) { (void)index; (void)hour; (void)minute; }
void model_set_alarm_repeat_mask_at(uint8_t index, uint8_t repeat_mask) { (void)index; (void)repeat_mask; }
void model_stopwatch_toggle(uint32_t now_ms) { (void)now_ms; }
void model_stopwatch_reset(void) {}
void model_stopwatch_lap(void) {}
void model_timer_toggle(uint32_t now_ms) { (void)now_ms; }
void model_timer_adjust_seconds(int32_t delta_s) { (void)delta_s; }
void model_timer_cycle_preset(int8_t dir) { (void)dir; }
void model_set_datetime(const DateTime *dt) { (void)dt; }
bool model_set_game_high_score(GameId game_id, uint16_t score) { (void)game_id; (void)score; return true; }

bool reset_service_reset_app_state(ResetActionReport *out) { (void)out; return true; }
bool reset_service_factory_reset(ResetActionReport *out) { (void)out; return true; }
bool sensor_service_force_reinit(void) { return true; }
void sensor_service_request_calibration(void) {}
void storage_service_request_commit(StorageCommitReason reason) { (void)reason; }
bool diag_service_can_clear_safe_mode(void) { return true; }
void diag_service_clear_safe_mode(void) {}

int main(void)
{
    const AppCommandDescriptor *canonical = app_command_describe(APP_COMMAND_RESET_APP_STATE);
    const AppCommandDescriptor *legacy = app_command_describe_by_name("restoreDefaults");
    const AppCommandDescriptor *legacy_catalog = app_command_describe(APP_COMMAND_RESTORE_DEFAULTS);

    assert(canonical != NULL);
    assert(strcmp(canonical->wire_name, "resetAppState") == 0);
    assert(legacy == canonical);
    assert(legacy_catalog != NULL);
    assert(strcmp(legacy_catalog->wire_name, "restoreDefaults") == 0);
    assert(legacy_catalog->web_exposed == false);
    puts("[OK] command catalog compatibility behavior check passed");
    return 0;
}

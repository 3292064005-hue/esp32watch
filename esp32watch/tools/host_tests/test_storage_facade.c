#include "services/storage_facade.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool g_flash_ready = false;
static bool g_has_token = false;
static bool g_restore_defaults_called = false;
static bool g_device_config_backend_ready = true;
static bool g_device_config_last_commit_ok = true;
static DeviceConfigSnapshot g_cfg = {0};

bool storage_service_is_flash_backend_ready(void) { return g_flash_ready; }
bool storage_service_app_state_durable_ready(void) { return g_flash_ready; }
const char *storage_service_get_backend_name(void) { return "RTC_RESET_DOMAIN"; }
const char *storage_service_backend_phase_name(void) { return "READY"; }
StorageCommitState storage_service_get_commit_state(void) { return STORAGE_COMMIT_STATE_IDLE; }
const char *storage_service_commit_state_name(StorageCommitState state) { return state == STORAGE_COMMIT_STATE_IDLE ? "IDLE" : "OTHER"; }
uint8_t storage_service_get_schema_version(void) { return 7U; }
bool storage_service_flash_supported(void) { return true; }
bool storage_service_was_migration_attempted(void) { return true; }
bool storage_service_last_migration_ok(void) { return true; }
bool storage_service_is_transaction_active(void) { return false; }
bool storage_service_is_sleep_flush_pending(void) { return false; }
void storage_service_load_settings(SettingsState *settings) { memset(settings, 0, sizeof(*settings)); settings->brightness = 2U; }
void storage_service_save_settings(const SettingsState *settings) { (void)settings; }
void storage_service_load_alarms(AlarmState *alarms, uint8_t count) { memset(alarms, 0, sizeof(*alarms) * count); }
void storage_service_save_alarms(const AlarmState *alarms, uint8_t count) { (void)alarms; (void)count; }
void storage_service_load_game_stats(GameStatsState *stats) { memset(stats, 0, sizeof(*stats)); }
void storage_service_save_game_stats(const GameStatsState *stats) { (void)stats; }
void storage_service_load_sensor_calibration(SensorCalibrationData *cal) { memset(cal, 0, sizeof(*cal)); }
void storage_service_save_sensor_calibration(const SensorCalibrationData *cal) { (void)cal; }
void storage_service_clear_sensor_calibration(void) {}

void device_config_init(void) {}
bool device_config_get(DeviceConfigSnapshot *out) { if (out == NULL) return false; *out = g_cfg; return true; }
bool device_config_apply_update(const DeviceConfigUpdate *update) { (void)update; return true; }
bool device_config_save_wifi(const char *ssid, const char *password) { (void)ssid; (void)password; return true; }
bool device_config_save_network_profile(const char *timezone_posix, const char *timezone_id, float latitude, float longitude, const char *location_name) { (void)timezone_posix; (void)timezone_id; (void)latitude; (void)longitude; (void)location_name; return true; }
bool device_config_save_api_token(const char *token) { (void)token; return true; }
bool device_config_get_wifi_password(char *out, uint32_t out_size) { if (out == NULL || out_size == 0U) return false; strncpy(out, "pw", out_size - 1U); out[out_size - 1U] = '\0'; return true; }
bool device_config_get_api_token(char *out, uint32_t out_size) { if (out == NULL || out_size == 0U) return false; strncpy(out, "ok", out_size - 1U); out[out_size - 1U] = '\0'; return true; }
bool device_config_has_api_token(void) { return g_has_token; }
bool device_config_authenticate_token(const char *token) { return token != NULL && strcmp(token, "ok") == 0; }
bool device_config_restore_defaults(void) { g_restore_defaults_called = true; return true; }

const char *device_config_backend_name(void) { return "NVS_A_B"; }
bool device_config_backend_ready(void) { return g_device_config_backend_ready; }
bool device_config_last_commit_ok(void) { return g_device_config_last_commit_ok; }

int main(void)
{
    StorageFacadeRuntimeSnapshot runtime = {0};
    StorageObjectSemantic semantic = {0};
    DeviceConfigSnapshot cfg = {0};
    char password[8] = {0};

    g_cfg.version = DEVICE_CONFIG_VERSION;
    strncpy(g_cfg.wifi_ssid, "watch-net", sizeof(g_cfg.wifi_ssid) - 1U);
    g_cfg.wifi_configured = true;
    g_flash_ready = true;
    g_has_token = true;

    assert(storage_facade_get_device_config(&cfg));
    assert(strcmp(cfg.wifi_ssid, "watch-net") == 0);
    assert(storage_facade_get_device_wifi_password(password, sizeof(password)));
    assert(strcmp(password, "pw") == 0);
    assert(storage_facade_device_config_has_api_token());
    assert(storage_facade_authenticate_device_token("ok"));
    assert(storage_facade_restore_device_config_defaults());
    assert(g_restore_defaults_called);

    assert(storage_facade_describe(STORAGE_OBJECT_APP_SETTINGS, &semantic));
    assert(strcmp(semantic.owner, "storage_service") == 0);
    assert(storage_facade_get_runtime_snapshot(&runtime));
    assert(strcmp(runtime.backend, "RTC_RESET_DOMAIN") == 0);
    assert(strcmp(runtime.backend_phase, "READY") == 0);
    assert(strcmp(runtime.commit_state, "IDLE") == 0);
    assert(runtime.schema_version == 7U);
    assert(strcmp(runtime.app_state_backend, "RTC_RESET_DOMAIN") == 0);
    assert(strcmp(runtime.device_config_backend, "NVS_A_B") == 0);
    assert(strcmp(runtime.app_state_durability, "DURABLE") == 0);
    assert(strcmp(runtime.device_config_durability, "DURABLE") == 0);
    assert(runtime.app_state_power_loss_guaranteed);
    assert(runtime.device_config_power_loss_guaranteed);
    assert(!runtime.app_state_mixed_durability);
    assert(runtime.app_state_reset_domain_object_count == 0U);
    assert(runtime.app_state_durable_object_count == 4U);
    assert(strcmp(runtime.device_config_backend, "NVS_A_B") == 0);
    g_device_config_backend_ready = false;
    g_device_config_last_commit_ok = false;
    assert(storage_facade_get_runtime_snapshot(&runtime));
    assert(!runtime.device_config_power_loss_guaranteed);

    g_device_config_backend_ready = true;
    g_device_config_last_commit_ok = true;

    puts("[OK] storage_facade behavior check passed");
    return 0;
}

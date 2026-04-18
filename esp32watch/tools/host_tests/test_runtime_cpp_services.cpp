#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"
#include "WiFi.h"

extern "C" {
#include "services/device_config.h"
#include "services/network_sync_service.h"
#include "services/runtime_event_service.h"
#include "services/time_service.h"
#include "web/web_wifi.h"
}

extern "C" const char _binary_data_certs_weather_ca_bundle_pem_start[] = "";
extern "C" const char _binary_data_certs_weather_ca_bundle_pem_end[] = "";

static DeviceConfigSnapshot g_cfg = {};
static bool g_cfg_ok = true;
static char g_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = "secret-pass";
static uint32_t g_config_generation = 9U;

extern "C" void storage_facade_device_config_init(void) {}
extern "C" bool storage_facade_get_device_config(DeviceConfigSnapshot *out)
{
    if (out == nullptr || !g_cfg_ok) {
        return false;
    }
    *out = g_cfg;
    return true;
}
extern "C" bool storage_facade_apply_device_config_update(const DeviceConfigUpdate *) { return false; }
extern "C" bool storage_facade_get_device_wifi_password(char *out, uint32_t out_size)
{
    if (out == nullptr || out_size == 0U || !g_cfg_ok) {
        return false;
    }
    strncpy(out, g_password, out_size - 1U);
    out[out_size - 1U] = '\0';
    return true;
}
extern "C" bool storage_facade_device_config_has_api_token(void) { return false; }
extern "C" bool storage_facade_authenticate_device_token(const char *) { return true; }
extern "C" bool storage_facade_restore_device_config_defaults(void) { return true; }
extern "C" uint32_t device_config_generation(void) { return g_config_generation; }
extern "C" bool time_service_init(void) { return true; }
extern "C" bool time_service_is_datetime_valid(const DateTime *) { return true; }
extern "C" uint32_t time_service_datetime_to_epoch(const DateTime *) { return 0U; }
extern "C" void time_service_epoch_to_datetime(uint32_t, DateTime *) {}
extern "C" uint32_t time_service_get_epoch(void) { return 0U; }
extern "C" void time_service_refresh(DateTime *) {}
extern "C" bool time_service_set_epoch_from_source(uint32_t, TimeSourceType) { return true; }
extern "C" bool time_service_try_set_datetime(const DateTime *, TimeSourceType) { return true; }
extern "C" void time_service_set_datetime(const DateTime *) {}
extern "C" void time_service_note_recovery_epoch(uint32_t, bool) {}
extern "C" bool time_service_get_source_snapshot(TimeSourceSnapshot *) { return false; }
extern "C" const char *time_service_source_name(TimeSourceType) { return "stub"; }
extern "C" const char *time_service_confidence_name(TimeConfidenceLevel) { return "stub"; }
extern "C" uint16_t time_service_day_id_from_epoch(uint32_t) { return 0U; }
extern "C" bool time_service_is_reasonable_epoch(uint32_t) { return true; }

static void seed_config(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.version = DEVICE_CONFIG_VERSION;
    g_cfg.wifi_configured = true;
    g_cfg.weather_configured = true;
    strncpy(g_cfg.wifi_ssid, "watch-net", sizeof(g_cfg.wifi_ssid) - 1U);
    strncpy(g_cfg.timezone_posix, "JST-9", sizeof(g_cfg.timezone_posix) - 1U);
    strncpy(g_cfg.timezone_id, "Asia/Tokyo", sizeof(g_cfg.timezone_id) - 1U);
    strncpy(g_cfg.location_name, "Tokyo", sizeof(g_cfg.location_name) - 1U);
    g_cfg.latitude = 35.6764f;
    g_cfg.longitude = 139.6500f;
    g_cfg_ok = true;
}

int main()
{
    runtime_event_service_reset();
    seed_config();
    host_stub_reset_wifi();
    host_stub_reset_arduino_io();

    assert(web_wifi_init());
    assert(network_sync_service_init());
    assert(runtime_event_service_handler_count() == 2U);
    assert(runtime_event_service_publish(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED));
    assert(runtime_event_service_publish_notify(RUNTIME_SERVICE_EVENT_POWER_STATE_CHANGED));
    assert(runtime_event_service_publish_notify(RUNTIME_SERVICE_EVENT_STORAGE_COMMIT_FINISHED));

    NetworkSyncSnapshot snapshot = {};
    assert(network_sync_service_get_snapshot(&snapshot));
    assert(strcmp(snapshot.weather_status, "PENDING") == 0 || strcmp(snapshot.weather_status, "UNSET") == 0);
    assert(web_wifi_last_applied_generation() == g_config_generation);
    assert(network_sync_service_last_applied_generation() == g_config_generation);
    assert(web_wifi_verify_config_applied(g_config_generation, &g_cfg));
    assert(network_sync_service_verify_config_applied(g_config_generation, &g_cfg));

#if defined(HOST_RUNTIME_TEST)
    network_sync_service_test_seed_weather_state(2U, "Osaka", "PENDING");
    network_sync_service_test_inject_weather_result(1U, true, "Tokyo", "SYNCED", 231, 1U, 1234U);
    network_sync_service_test_apply_pending_weather_result();
    assert(network_sync_service_get_snapshot(&snapshot));
    assert(strcmp(snapshot.location_name, "Osaka") == 0);
    assert(strcmp(snapshot.weather_status, "PENDING") == 0);

    network_sync_service_test_inject_weather_result(2U, true, "Osaka", "SYNCED", 245, 2U, 2345U);
    network_sync_service_test_apply_pending_weather_result();
    assert(network_sync_service_get_snapshot(&snapshot));
    assert(strcmp(snapshot.location_name, "Osaka") == 0);
    assert(strcmp(snapshot.weather_status, "SYNCED") == 0);
    assert(snapshot.temperature_tenths_c == 245);
#endif

    g_cfg_ok = false;
    assert(!runtime_event_service_publish(RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED));

    puts("[OK] runtime C++ service integration check passed");
    return 0;
}

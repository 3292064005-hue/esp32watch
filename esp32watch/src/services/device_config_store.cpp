#include <math.h>
#include <string.h>

extern "C" {
#include "services/storage_facade.h"
}

#include "services/device_config_internal.hpp"

bool persist_snapshot(const DeviceConfigSnapshot &snapshot,
                      const char *wifi_password,
                      const char *api_token)
{
    DeviceConfigStorageRecord record = {};
    uint8_t target_slot;
    bool ok = false;

    if (!validate_wifi_fields(snapshot.wifi_ssid, wifi_password) ||
        !validate_network_profile_fields(snapshot.timezone_posix, snapshot.timezone_id, snapshot.latitude, snapshot.longitude, snapshot.location_name) ||
        !validate_api_token_field(api_token)) {
        g_last_commit_ok = false;
        return false;
    }

    if (!prefs_open(false)) {
        g_backend_ready = false;
        g_last_commit_ok = false;
        return false;
    }

    target_slot = (g_active_slot == 0U) ? 1U : 0U;
    fill_record(&record, snapshot, wifi_password, api_token, g_generation + 1U);
    ok = persist_record_locked(target_slot, record);
    if (ok) {
        load_runtime_from_record(record, target_slot);
    }
    g_prefs.end();
    g_backend_ready = ok;
    g_last_commit_ok = ok;
    return ok;
}

void load_from_preferences(void)
{
    DeviceConfigStorageRecord record = {};
    uint8_t slot = kInvalidSlot;
    DeviceConfigSnapshot legacy_snapshot = {};
    char legacy_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {};
    char legacy_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {};

    if (!prefs_open(false)) {
        g_backend_ready = false;
        seed_defaults();
        g_loaded = true;
        return;
    }

    g_backend_ready = true;
    if (load_from_slots_locked(&record, &slot)) {
        load_runtime_from_record(record, slot);
        g_last_commit_ok = true;
        g_prefs.end();
        g_loaded = true;
        return;
    }

    seed_defaults();
    if (load_legacy_preferences_locked(&legacy_snapshot,
                                       legacy_wifi_password,
                                       sizeof(legacy_wifi_password),
                                       legacy_api_token,
                                       sizeof(legacy_api_token))) {
        g_snapshot = legacy_snapshot;
        copy_string(g_wifi_password, sizeof(g_wifi_password), legacy_wifi_password);
        copy_string(g_api_token, sizeof(g_api_token), legacy_api_token);
        update_derived_flags();
        g_last_commit_ok = persist_snapshot(g_snapshot, g_wifi_password, g_api_token);
    } else {
        g_last_commit_ok = true;
    }

    g_prefs.end();
    g_loaded = true;
}

void seed_update_from_current(DeviceConfigUpdate *update)
{
    if (update == nullptr) {
        return;
    }

    memset(update, 0, sizeof(*update));
    copy_string(update->wifi_ssid, sizeof(update->wifi_ssid), g_snapshot.wifi_ssid);
    copy_string(update->wifi_password, sizeof(update->wifi_password), g_wifi_password);
    copy_string(update->timezone_posix, sizeof(update->timezone_posix), g_snapshot.timezone_posix);
    copy_string(update->timezone_id, sizeof(update->timezone_id), g_snapshot.timezone_id);
    copy_string(update->location_name, sizeof(update->location_name), g_snapshot.location_name);
    copy_string(update->api_token, sizeof(update->api_token), g_api_token);
    update->latitude = g_snapshot.latitude;
    update->longitude = g_snapshot.longitude;
}

#include <Preferences.h>
#include <math.h>
#include <string.h>

extern "C" {
#include "services/device_config.h"
#include "common/crc16.h"
#include "persist_namespace_config.h"
}
#include "persist_preferences.h"
#include "services/device_config_codec.h"
#include "services/device_config_backend.h"

#include "services/device_config_internal.hpp"

Preferences g_prefs;
bool g_loaded = false;
bool g_backend_ready = false;
bool g_last_commit_ok = false;
DeviceConfigSnapshot g_snapshot = {};
char g_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
char g_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {0};
uint8_t g_active_slot = kInvalidSlot;
uint32_t g_generation = 0U;

extern "C" void device_config_init(void)
{
    if (!g_loaded) {
        load_from_preferences();
    }
}

extern "C" bool device_config_get(DeviceConfigSnapshot *out)
{
    device_config_init();
    if (out == nullptr) {
        return false;
    }
    *out = g_snapshot;
    return true;
}

extern "C" bool device_config_backend_apply_update(const DeviceConfigUpdate *update)
{
    DeviceConfigSnapshot next_snapshot;
    DeviceConfigUpdate canonical_update = {};
    char next_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
    char next_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {0};

    device_config_init();
    if (update == nullptr) {
        return false;
    }

    canonical_update = *update;
    device_config_canonicalize_update(&canonical_update);

    next_snapshot = g_snapshot;
    copy_string(next_wifi_password, sizeof(next_wifi_password), g_wifi_password);
    copy_string(next_api_token, sizeof(next_api_token), g_api_token);

    if (canonical_update.set_wifi) {
        if (!validate_wifi_fields(canonical_update.wifi_ssid, canonical_update.wifi_password)) {
            return false;
        }
        copy_string(next_snapshot.wifi_ssid, sizeof(next_snapshot.wifi_ssid), canonical_update.wifi_ssid);
        copy_string(next_wifi_password, sizeof(next_wifi_password), canonical_update.wifi_password);
        if (canonical_update.wifi_ssid[0] == '\0') {
            next_wifi_password[0] = '\0';
        }
    }

    if (canonical_update.set_network) {
        if (!validate_network_profile_fields(canonical_update.timezone_posix,
                                             canonical_update.timezone_id,
                                             canonical_update.latitude,
                                             canonical_update.longitude,
                                             canonical_update.location_name)) {
            return false;
        }
        copy_string(next_snapshot.timezone_posix, sizeof(next_snapshot.timezone_posix), canonical_update.timezone_posix);
        copy_string(next_snapshot.timezone_id, sizeof(next_snapshot.timezone_id), canonical_update.timezone_id);
        copy_string(next_snapshot.location_name, sizeof(next_snapshot.location_name), canonical_update.location_name);
        next_snapshot.latitude = canonical_update.latitude;
        next_snapshot.longitude = canonical_update.longitude;
    }

    if (canonical_update.set_api_token) {
        if (!validate_api_token_field(canonical_update.api_token)) {
            return false;
        }
        copy_string(next_api_token, sizeof(next_api_token), canonical_update.api_token);
    }

    return persist_snapshot(next_snapshot, next_wifi_password, next_api_token);
}


extern "C" bool device_config_backend_restore_defaults(void)
{
    DeviceConfigSnapshot defaults_snapshot;
    char default_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
    char default_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {0};

    device_config_init();
    seed_defaults_snapshot(&defaults_snapshot,
                           default_wifi_password,
                           sizeof(default_wifi_password),
                           default_api_token,
                           sizeof(default_api_token));
    return persist_snapshot(defaults_snapshot, default_wifi_password, default_api_token);
}


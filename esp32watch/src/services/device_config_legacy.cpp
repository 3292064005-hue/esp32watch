#include <math.h>
#include <string.h>

extern "C" {
#include "services/storage_facade.h"
}

#include "services/device_config_internal.hpp"

bool load_legacy_preferences_locked(DeviceConfigSnapshot *snapshot,
                                    char *wifi_password,
                                    size_t wifi_password_size,
                                    char *api_token,
                                    size_t api_token_size)
{
    if (snapshot == nullptr || wifi_password == nullptr || api_token == nullptr) {
        return false;
    }

    seed_defaults_snapshot(snapshot, wifi_password, wifi_password_size, api_token, api_token_size);
    snapshot->version = DEVICE_CONFIG_VERSION;
    copy_string(snapshot->wifi_ssid, sizeof(snapshot->wifi_ssid), g_prefs.getString(kKeyWifiSsid, "").c_str());
    copy_string(wifi_password, wifi_password_size, g_prefs.getString(kKeyWifiPassword, "").c_str());
    copy_string(snapshot->timezone_posix, sizeof(snapshot->timezone_posix), g_prefs.getString(kKeyTimezonePosix, kDefaultTimezonePosix).c_str());
    copy_string(snapshot->timezone_id, sizeof(snapshot->timezone_id), g_prefs.getString(kKeyTimezoneId, kDefaultTimezoneId).c_str());
    copy_string(snapshot->location_name, sizeof(snapshot->location_name), g_prefs.getString(kKeyLocationName, kDefaultLocationName).c_str());
    copy_string(api_token, api_token_size, g_prefs.getString(kKeyApiToken, "").c_str());
    snapshot->latitude = g_prefs.getFloat(kKeyLatitude, 0.0f);
    snapshot->longitude = g_prefs.getFloat(kKeyLongitude, 0.0f);
    update_derived_flags_for(snapshot, api_token);

    return snapshot->wifi_ssid[0] != '\0' || wifi_password[0] != '\0' ||
           snapshot->timezone_posix[0] != '\0' || snapshot->timezone_id[0] != '\0' ||
           snapshot->location_name[0] != '\0' || api_token[0] != '\0' ||
           snapshot->latitude != 0.0f || snapshot->longitude != 0.0f;
}

#include <math.h>
#include <string.h>

#include "persist_preferences.h"

extern "C" {
#include "services/device_config_codec.h"
}

#include "services/device_config_internal.hpp"

bool prefs_open(bool read_only)
{
    return persist_preferences_begin(g_prefs, PERSIST_PREFS_DOMAIN_DEVICE_CONFIG, read_only);
}

void copy_string(char *dst, size_t dst_size, const char *src)
{
    device_config_copy_string(dst, dst_size, src);
}

bool string_length_valid(const char *value, size_t max_len)
{
    return value != nullptr && strnlen(value, max_len + 1U) <= max_len;
}

bool float_is_finite(float value)
{
    return isfinite(value);
}

bool weather_configured_from_snapshot(const DeviceConfigSnapshot &snapshot)
{
    return snapshot.latitude != 0.0f && snapshot.longitude != 0.0f && snapshot.location_name[0] != '\0';
}

void seed_defaults_snapshot(DeviceConfigSnapshot *snapshot,
                            char *wifi_password,
                            size_t wifi_password_size,
                            char *api_token,
                            size_t api_token_size)
{
    if (snapshot == nullptr || wifi_password == nullptr || api_token == nullptr) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->version = DEVICE_CONFIG_VERSION;
    copy_string(snapshot->timezone_posix, sizeof(snapshot->timezone_posix), kDefaultTimezonePosix);
    copy_string(snapshot->timezone_id, sizeof(snapshot->timezone_id), kDefaultTimezoneId);
    copy_string(snapshot->location_name, sizeof(snapshot->location_name), kDefaultLocationName);
    copy_string(wifi_password, wifi_password_size, "");
    copy_string(api_token, api_token_size, "");
    snapshot->latitude = 0.0f;
    snapshot->longitude = 0.0f;
    snapshot->weather_configured = false;
    snapshot->wifi_configured = false;
    snapshot->api_token_configured = false;
}

void seed_defaults(void)
{
    seed_defaults_snapshot(&g_snapshot, g_wifi_password, sizeof(g_wifi_password), g_api_token, sizeof(g_api_token));
    g_active_slot = kInvalidSlot;
    g_generation = 1U;
    g_last_commit_ok = false;
}

void update_derived_flags_for(DeviceConfigSnapshot *snapshot, const char *api_token)
{
    if (snapshot == nullptr) {
        return;
    }
    snapshot->wifi_configured = snapshot->wifi_ssid[0] != '\0';
    snapshot->weather_configured = weather_configured_from_snapshot(*snapshot);
    snapshot->api_token_configured = api_token != nullptr && api_token[0] != '\0';
}

void update_derived_flags(void)
{
    update_derived_flags_for(&g_snapshot, g_api_token);
}

bool validate_wifi_fields(const char *ssid, const char *password)
{
    if (!string_length_valid(ssid, DEVICE_CONFIG_WIFI_SSID_MAX_LEN) ||
        !string_length_valid(password, DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN)) {
        return false;
    }
    size_t password_len = strnlen(password, DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U);
    return password_len == 0U || password_len >= 8U;
}

bool validate_network_profile_fields(const char *timezone_posix,
                                     const char *timezone_id,
                                     float latitude,
                                     float longitude,
                                     const char *location_name)
{
    return string_length_valid(timezone_posix, DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN) &&
           string_length_valid(timezone_id, DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN) &&
           string_length_valid(location_name, DEVICE_CONFIG_LOCATION_NAME_MAX_LEN) &&
           float_is_finite(latitude) && float_is_finite(longitude) &&
           latitude >= -90.0f && latitude <= 90.0f &&
           longitude >= -180.0f && longitude <= 180.0f;
}

bool validate_api_token_field(const char *token)
{
    return string_length_valid(token, DEVICE_CONFIG_API_TOKEN_MAX_LEN);
}

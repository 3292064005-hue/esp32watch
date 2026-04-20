#ifndef DEVICE_CONFIG_INTERNAL_HPP
#define DEVICE_CONFIG_INTERNAL_HPP

#include <Preferences.h>
#include <stddef.h>

extern "C" {
#include "services/device_config.h"
}

inline constexpr const char *kKeyActiveSlot = "active_slot";
inline constexpr const char *kKeyRecordA = "cfg_a";
inline constexpr const char *kKeyRecordB = "cfg_b";
inline constexpr const char *kKeyVersion = "version";
inline constexpr const char *kKeyWifiSsid = "wifi_ssid";
inline constexpr const char *kKeyWifiPassword = "wifi_pass";
inline constexpr const char *kKeyTimezonePosix = "tz_posix";
inline constexpr const char *kKeyTimezoneId = "tz_id";
inline constexpr const char *kKeyLatitude = "lat";
inline constexpr const char *kKeyLongitude = "lon";
inline constexpr const char *kKeyLocationName = "location";
inline constexpr const char *kKeyApiToken = "api_token";
inline constexpr const char *kDefaultTimezonePosix = "UTC0";
inline constexpr const char *kDefaultTimezoneId = "Etc/UTC";
inline constexpr const char *kDefaultLocationName = "UNSET";
inline constexpr uint32_t kRecordMagic = 0x57434647UL;
inline constexpr uint8_t kInvalidSlot = 0xFFU;

struct DeviceConfigStorageRecord {
    uint32_t magic;
    uint16_t schema_version;
    uint16_t payload_version;
    uint32_t generation;
    char wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX_LEN + 1U];
    char wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U];
    char timezone_posix[DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN + 1U];
    char timezone_id[DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN + 1U];
    char location_name[DEVICE_CONFIG_LOCATION_NAME_MAX_LEN + 1U];
    char api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U];
    float latitude;
    float longitude;
    uint16_t crc16;
};

extern Preferences g_prefs;
extern bool g_loaded;
extern bool g_backend_ready;
extern bool g_last_commit_ok;
extern DeviceConfigSnapshot g_snapshot;
extern char g_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U];
extern char g_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U];
extern uint8_t g_active_slot;
extern uint32_t g_generation;

bool prefs_open(bool read_only);
void copy_string(char *dst, size_t dst_size, const char *src);
bool string_length_valid(const char *value, size_t max_len);
bool float_is_finite(float value);
bool weather_configured_from_snapshot(const DeviceConfigSnapshot &snapshot);
void seed_defaults_snapshot(DeviceConfigSnapshot *snapshot,
                            char *wifi_password,
                            size_t wifi_password_size,
                            char *api_token,
                            size_t api_token_size);
void seed_defaults(void);
void update_derived_flags_for(DeviceConfigSnapshot *snapshot, const char *api_token);
void update_derived_flags(void);
uint16_t record_crc16(const DeviceConfigStorageRecord &record);
void fill_record(DeviceConfigStorageRecord *record,
                 const DeviceConfigSnapshot &snapshot,
                 const char *wifi_password,
                 const char *api_token,
                 uint32_t generation);
bool record_is_valid(const DeviceConfigStorageRecord &record);
void load_runtime_from_record(const DeviceConfigStorageRecord &record, uint8_t slot);
const char *slot_key(uint8_t slot);
bool read_record_locked(uint8_t slot, DeviceConfigStorageRecord *out);
void remove_legacy_keys_locked(void);
bool persist_record_locked(uint8_t target_slot, const DeviceConfigStorageRecord &record);
bool validate_wifi_fields(const char *ssid, const char *password);
bool validate_network_profile_fields(const char *timezone_posix,
                                     const char *timezone_id,
                                     float latitude,
                                     float longitude,
                                     const char *location_name);
bool validate_api_token_field(const char *token);
bool load_legacy_preferences_locked(DeviceConfigSnapshot *snapshot,
                                    char *wifi_password,
                                    size_t wifi_password_size,
                                    char *api_token,
                                    size_t api_token_size);
bool load_from_slots_locked(DeviceConfigStorageRecord *best_record, uint8_t *best_slot);
bool persist_snapshot(const DeviceConfigSnapshot &snapshot,
                      const char *wifi_password,
                      const char *api_token);
void load_from_preferences(void);
void seed_update_from_current(DeviceConfigUpdate *update);

#endif

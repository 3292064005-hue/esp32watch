#include <Preferences.h>
#include <math.h>
#include <string.h>

extern "C" {
#include "services/device_config.h"
#include "common/crc16.h"
#include "persist_namespace_config.h"
}
#include "persist_preferences.h"

namespace {
constexpr const char *kKeyActiveSlot = "active_slot";
constexpr const char *kKeyRecordA = "cfg_a";
constexpr const char *kKeyRecordB = "cfg_b";
constexpr const char *kKeyVersion = "version";
constexpr const char *kKeyWifiSsid = "wifi_ssid";
constexpr const char *kKeyWifiPassword = "wifi_pass";
constexpr const char *kKeyTimezonePosix = "tz_posix";
constexpr const char *kKeyTimezoneId = "tz_id";
constexpr const char *kKeyLatitude = "lat";
constexpr const char *kKeyLongitude = "lon";
constexpr const char *kKeyLocationName = "location";
constexpr const char *kKeyApiToken = "api_token";
constexpr const char *kDefaultTimezonePosix = "UTC0";
constexpr const char *kDefaultTimezoneId = "Etc/UTC";
constexpr const char *kDefaultLocationName = "UNSET";
constexpr uint32_t kRecordMagic = 0x57434647UL; /* WCFG */
constexpr uint8_t kInvalidSlot = 0xFFU;

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

Preferences g_prefs;
bool g_loaded = false;
bool g_backend_ready = false;
bool g_last_commit_ok = false;
DeviceConfigSnapshot g_snapshot = {};
char g_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
char g_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {0};
uint8_t g_active_slot = kInvalidSlot;
uint32_t g_generation = 0U;

static bool prefs_open(bool read_only)
{
    return persist_preferences_begin(g_prefs, PERSIST_PREFS_DOMAIN_DEVICE_CONFIG, read_only);
}

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst == nullptr || dst_size == 0U) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

static bool string_length_valid(const char *value, size_t max_len)
{
    return value != nullptr && strlen(value) <= max_len;
}

static bool float_is_finite(float value)
{
    return isfinite(value) != 0;
}

static bool weather_configured_from_snapshot(const DeviceConfigSnapshot &snapshot)
{
    return snapshot.location_name[0] != '\0' &&
           snapshot.location_name[0] != ' ' &&
           float_is_finite(snapshot.latitude) &&
           float_is_finite(snapshot.longitude) &&
           snapshot.latitude >= -90.0f && snapshot.latitude <= 90.0f &&
           snapshot.longitude >= -180.0f && snapshot.longitude <= 180.0f;
}

static void seed_defaults_snapshot(DeviceConfigSnapshot *snapshot,
                                   char *wifi_password,
                                   size_t wifi_password_size,
                                   char *api_token,
                                   size_t api_token_size)
{
    if (snapshot == nullptr || wifi_password == nullptr || api_token == nullptr) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    memset(wifi_password, 0, wifi_password_size);
    memset(api_token, 0, api_token_size);

    snapshot->version = DEVICE_CONFIG_VERSION;
    copy_string(snapshot->timezone_posix, sizeof(snapshot->timezone_posix), kDefaultTimezonePosix);
    copy_string(snapshot->timezone_id, sizeof(snapshot->timezone_id), kDefaultTimezoneId);
    copy_string(snapshot->location_name, sizeof(snapshot->location_name), kDefaultLocationName);
    snapshot->latitude = 0.0f;
    snapshot->longitude = 0.0f;
    snapshot->wifi_configured = false;
    snapshot->weather_configured = false;
    snapshot->api_token_configured = false;
}

static void seed_defaults(void)
{
    seed_defaults_snapshot(&g_snapshot, g_wifi_password, sizeof(g_wifi_password), g_api_token, sizeof(g_api_token));
    g_active_slot = kInvalidSlot;
    g_generation = 0U;
}

static void update_derived_flags_for(DeviceConfigSnapshot *snapshot, const char *api_token)
{
    if (snapshot == nullptr || api_token == nullptr) {
        return;
    }

    snapshot->wifi_configured = snapshot->wifi_ssid[0] != '\0';
    snapshot->weather_configured = weather_configured_from_snapshot(*snapshot) &&
                                   strcmp(snapshot->location_name, kDefaultLocationName) != 0;
    snapshot->api_token_configured = api_token[0] != '\0';
    snapshot->version = DEVICE_CONFIG_VERSION;
}

static void update_derived_flags(void)
{
    update_derived_flags_for(&g_snapshot, g_api_token);
}

static uint16_t record_crc16(const DeviceConfigStorageRecord &record)
{
    return crc16_buf(reinterpret_cast<const uint8_t *>(&record), (uint32_t)(sizeof(record) - sizeof(record.crc16)));
}

static void fill_record(DeviceConfigStorageRecord *record,
                        const DeviceConfigSnapshot &snapshot,
                        const char *wifi_password,
                        const char *api_token,
                        uint32_t generation)
{
    DeviceConfigSnapshot normalized = snapshot;

    memset(record, 0, sizeof(*record));
    update_derived_flags_for(&normalized, api_token);
    record->magic = kRecordMagic;
    record->schema_version = DEVICE_CONFIG_STORAGE_SCHEMA_VERSION;
    record->payload_version = DEVICE_CONFIG_VERSION;
    record->generation = generation;
    copy_string(record->wifi_ssid, sizeof(record->wifi_ssid), normalized.wifi_ssid);
    copy_string(record->wifi_password, sizeof(record->wifi_password), wifi_password);
    copy_string(record->timezone_posix, sizeof(record->timezone_posix), normalized.timezone_posix);
    copy_string(record->timezone_id, sizeof(record->timezone_id), normalized.timezone_id);
    copy_string(record->location_name, sizeof(record->location_name), normalized.location_name);
    copy_string(record->api_token, sizeof(record->api_token), api_token);
    record->latitude = normalized.latitude;
    record->longitude = normalized.longitude;
    record->crc16 = record_crc16(*record);
}

static bool record_is_valid(const DeviceConfigStorageRecord &record)
{
    if (record.magic != kRecordMagic ||
        record.schema_version != DEVICE_CONFIG_STORAGE_SCHEMA_VERSION ||
        record.payload_version != DEVICE_CONFIG_VERSION) {
        return false;
    }
    if (!string_length_valid(record.wifi_ssid, DEVICE_CONFIG_WIFI_SSID_MAX_LEN) ||
        !string_length_valid(record.wifi_password, DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN) ||
        !string_length_valid(record.timezone_posix, DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN) ||
        !string_length_valid(record.timezone_id, DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN) ||
        !string_length_valid(record.location_name, DEVICE_CONFIG_LOCATION_NAME_MAX_LEN) ||
        !string_length_valid(record.api_token, DEVICE_CONFIG_API_TOKEN_MAX_LEN)) {
        return false;
    }
    if (!float_is_finite(record.latitude) || !float_is_finite(record.longitude)) {
        return false;
    }
    return record.crc16 == record_crc16(record);
}

static void load_runtime_from_record(const DeviceConfigStorageRecord &record, uint8_t slot)
{
    seed_defaults();
    g_snapshot.version = record.payload_version;
    copy_string(g_snapshot.wifi_ssid, sizeof(g_snapshot.wifi_ssid), record.wifi_ssid);
    copy_string(g_wifi_password, sizeof(g_wifi_password), record.wifi_password);
    copy_string(g_snapshot.timezone_posix, sizeof(g_snapshot.timezone_posix), record.timezone_posix);
    copy_string(g_snapshot.timezone_id, sizeof(g_snapshot.timezone_id), record.timezone_id);
    copy_string(g_snapshot.location_name, sizeof(g_snapshot.location_name), record.location_name);
    g_snapshot.latitude = record.latitude;
    g_snapshot.longitude = record.longitude;
    copy_string(g_api_token, sizeof(g_api_token), record.api_token);
    update_derived_flags();
    g_generation = record.generation;
    g_active_slot = slot;
}

static const char *slot_key(uint8_t slot)
{
    return slot == 0U ? kKeyRecordA : kKeyRecordB;
}

static bool read_record_locked(uint8_t slot, DeviceConfigStorageRecord *out)
{
    size_t stored_len;
    if (slot > 1U || out == nullptr) {
        return false;
    }
    stored_len = g_prefs.getBytesLength(slot_key(slot));
    if (stored_len != sizeof(DeviceConfigStorageRecord)) {
        return false;
    }
    return g_prefs.getBytes(slot_key(slot), out, sizeof(DeviceConfigStorageRecord)) == sizeof(DeviceConfigStorageRecord);
}

static void remove_legacy_keys_locked(void)
{
    g_prefs.remove(kKeyVersion);
    g_prefs.remove(kKeyWifiSsid);
    g_prefs.remove(kKeyWifiPassword);
    g_prefs.remove(kKeyTimezonePosix);
    g_prefs.remove(kKeyTimezoneId);
    g_prefs.remove(kKeyLatitude);
    g_prefs.remove(kKeyLongitude);
    g_prefs.remove(kKeyLocationName);
    g_prefs.remove(kKeyApiToken);
}

static bool persist_record_locked(uint8_t target_slot, const DeviceConfigStorageRecord &record)
{
    bool ok = false;
    if (target_slot > 1U) {
        return false;
    }

    ok = g_prefs.putBytes(slot_key(target_slot), &record, sizeof(record)) == sizeof(record);
    ok = ok && (g_prefs.putUChar(kKeyActiveSlot, target_slot) == sizeof(uint8_t));
    if (!ok) {
        return false;
    }

    remove_legacy_keys_locked();
    return true;
}

static bool validate_wifi_fields(const char *ssid, const char *password)
{
    size_t password_len;

    if (ssid == nullptr || password == nullptr) {
        return false;
    }
    if (!string_length_valid(ssid, DEVICE_CONFIG_WIFI_SSID_MAX_LEN) ||
        !string_length_valid(password, DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN)) {
        return false;
    }

    password_len = strlen(password);
    if (ssid[0] != '\0' && password_len > 0U && password_len < 8U) {
        return false;
    }
    return true;
}

static bool validate_network_profile_fields(const char *timezone_posix,
                                            const char *timezone_id,
                                            float latitude,
                                            float longitude,
                                            const char *location_name)
{
    if (timezone_posix == nullptr || timezone_id == nullptr || location_name == nullptr) {
        return false;
    }
    if (!string_length_valid(timezone_posix, DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN) ||
        !string_length_valid(timezone_id, DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN) ||
        !string_length_valid(location_name, DEVICE_CONFIG_LOCATION_NAME_MAX_LEN)) {
        return false;
    }
    if (timezone_posix[0] == '\0' || timezone_id[0] == '\0' || location_name[0] == '\0') {
        return false;
    }
    if (!float_is_finite(latitude) || !float_is_finite(longitude) ||
        latitude < -90.0f || latitude > 90.0f ||
        longitude < -180.0f || longitude > 180.0f) {
        return false;
    }
    return true;
}

static bool validate_api_token_field(const char *token)
{
    return token != nullptr && string_length_valid(token, DEVICE_CONFIG_API_TOKEN_MAX_LEN);
}

static bool load_legacy_preferences_locked(DeviceConfigSnapshot *snapshot,
                                           char *wifi_password,
                                           size_t wifi_password_size,
                                           char *api_token,
                                           size_t api_token_size)
{
    String value;
    bool has_any_legacy = false;

    if (snapshot == nullptr || wifi_password == nullptr || api_token == nullptr) {
        return false;
    }

    seed_defaults_snapshot(snapshot, wifi_password, wifi_password_size, api_token, api_token_size);

    if (g_prefs.isKey(kKeyVersion)) {
        snapshot->version = (uint16_t)g_prefs.getUShort(kKeyVersion, DEVICE_CONFIG_VERSION);
        has_any_legacy = true;
    }

    value = g_prefs.getString(kKeyWifiSsid, "");
    if (value.length() > 0) {
        copy_string(snapshot->wifi_ssid, sizeof(snapshot->wifi_ssid), value.c_str());
        has_any_legacy = true;
    }

    value = g_prefs.getString(kKeyWifiPassword, "");
    if (value.length() > 0) {
        copy_string(wifi_password, wifi_password_size, value.c_str());
        has_any_legacy = true;
    }

    value = g_prefs.getString(kKeyTimezonePosix, kDefaultTimezonePosix);
    if (value.length() > 0) {
        copy_string(snapshot->timezone_posix, sizeof(snapshot->timezone_posix), value.c_str());
        has_any_legacy = true;
    }

    value = g_prefs.getString(kKeyTimezoneId, kDefaultTimezoneId);
    if (value.length() > 0) {
        copy_string(snapshot->timezone_id, sizeof(snapshot->timezone_id), value.c_str());
        has_any_legacy = true;
    }

    value = g_prefs.getString(kKeyLocationName, kDefaultLocationName);
    if (value.length() > 0) {
        copy_string(snapshot->location_name, sizeof(snapshot->location_name), value.c_str());
        has_any_legacy = true;
    }

    if (g_prefs.isKey(kKeyLatitude)) {
        snapshot->latitude = g_prefs.getFloat(kKeyLatitude, 0.0f);
        has_any_legacy = true;
    }
    if (g_prefs.isKey(kKeyLongitude)) {
        snapshot->longitude = g_prefs.getFloat(kKeyLongitude, 0.0f);
        has_any_legacy = true;
    }

    value = g_prefs.getString(kKeyApiToken, "");
    if (value.length() > 0) {
        copy_string(api_token, api_token_size, value.c_str());
        has_any_legacy = true;
    }

    update_derived_flags_for(snapshot, api_token);
    return has_any_legacy;
}

static bool load_from_slots_locked(DeviceConfigStorageRecord *best_record, uint8_t *best_slot)
{
    DeviceConfigStorageRecord record_a = {};
    DeviceConfigStorageRecord record_b = {};
    bool has_a = read_record_locked(0U, &record_a) && record_is_valid(record_a);
    bool has_b = read_record_locked(1U, &record_b) && record_is_valid(record_b);
    uint8_t active_slot = g_prefs.getUChar(kKeyActiveSlot, kInvalidSlot);

    if (best_record == nullptr || best_slot == nullptr) {
        return false;
    }

    if (active_slot <= 1U) {
        DeviceConfigStorageRecord active_record = {};
        if (read_record_locked(active_slot, &active_record) && record_is_valid(active_record)) {
            *best_record = active_record;
            *best_slot = active_slot;
            return true;
        }
    }

    if (has_a && has_b) {
        if (record_b.generation > record_a.generation) {
            *best_record = record_b;
            *best_slot = 1U;
        } else {
            *best_record = record_a;
            *best_slot = 0U;
        }
        return true;
    }
    if (has_a) {
        *best_record = record_a;
        *best_slot = 0U;
        return true;
    }
    if (has_b) {
        *best_record = record_b;
        *best_slot = 1U;
        return true;
    }
    return false;
}

static bool persist_snapshot(const DeviceConfigSnapshot &snapshot,
                             const char *wifi_password,
                             const char *api_token)
{
    DeviceConfigStorageRecord record = {};
    DeviceConfigSnapshot normalized = snapshot;
    uint8_t next_slot;
    uint32_t next_generation;

    if (wifi_password == nullptr || api_token == nullptr) {
        return false;
    }

    update_derived_flags_for(&normalized, api_token);
    next_slot = (g_active_slot == 0U) ? 1U : 0U;
    next_generation = g_generation + 1U;
    fill_record(&record, normalized, wifi_password, api_token, next_generation);

    if (!prefs_open(false)) {
        g_backend_ready = false;
        g_last_commit_ok = false;
        return false;
    }
    bool ok = persist_record_locked(next_slot, record);
    g_prefs.end();
    g_backend_ready = true;
    g_last_commit_ok = ok;
    if (!ok) {
        return false;
    }

    g_snapshot = normalized;
    copy_string(g_wifi_password, sizeof(g_wifi_password), wifi_password);
    copy_string(g_api_token, sizeof(g_api_token), api_token);
    update_derived_flags();
    g_active_slot = next_slot;
    g_generation = next_generation;
    return true;
}

static void load_from_preferences(void)
{
    DeviceConfigStorageRecord record = {};
    DeviceConfigSnapshot legacy_snapshot = {};
    char legacy_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
    char legacy_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {0};
    uint8_t slot = kInvalidSlot;
    bool loaded_from_slots = false;
    bool loaded_from_legacy = false;

    seed_defaults();
    if (!prefs_open(true)) {
        g_backend_ready = false;
        g_last_commit_ok = false;
        g_loaded = true;
        return;
    }

    g_backend_ready = true;

    loaded_from_slots = load_from_slots_locked(&record, &slot);
    if (!loaded_from_slots) {
        loaded_from_legacy = load_legacy_preferences_locked(&legacy_snapshot,
                                                            legacy_wifi_password,
                                                            sizeof(legacy_wifi_password),
                                                            legacy_api_token,
                                                            sizeof(legacy_api_token));
    }
    g_prefs.end();

    if (loaded_from_slots) {
        load_runtime_from_record(record, slot);
        g_last_commit_ok = true;
    } else if (loaded_from_legacy) {
        g_snapshot = legacy_snapshot;
        copy_string(g_wifi_password, sizeof(g_wifi_password), legacy_wifi_password);
        copy_string(g_api_token, sizeof(g_api_token), legacy_api_token);
        update_derived_flags();
        g_last_commit_ok = persist_snapshot(g_snapshot, g_wifi_password, g_api_token);
    } else {
        g_last_commit_ok = true;
    }

    g_loaded = true;
}

static void seed_update_from_current(DeviceConfigUpdate *update)
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

static bool constant_time_equals(const char *lhs, const char *rhs)
{
    size_t lhs_len;
    size_t rhs_len;
    size_t max_len;
    uint8_t diff = 0U;

    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }

    lhs_len = strlen(lhs);
    rhs_len = strlen(rhs);
    max_len = lhs_len > rhs_len ? lhs_len : rhs_len;
    for (size_t i = 0U; i < max_len; ++i) {
        const uint8_t lhs_ch = i < lhs_len ? (uint8_t)lhs[i] : 0U;
        const uint8_t rhs_ch = i < rhs_len ? (uint8_t)rhs[i] : 0U;
        diff |= (uint8_t)(lhs_ch ^ rhs_ch);
    }
    diff |= (uint8_t)(lhs_len ^ rhs_len);
    return diff == 0U;
}
} // namespace

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

extern "C" bool device_config_apply_update(const DeviceConfigUpdate *update)
{
    DeviceConfigSnapshot next_snapshot;
    char next_wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN + 1U] = {0};
    char next_api_token[DEVICE_CONFIG_API_TOKEN_MAX_LEN + 1U] = {0};

    device_config_init();
    if (update == nullptr) {
        return false;
    }

    next_snapshot = g_snapshot;
    copy_string(next_wifi_password, sizeof(next_wifi_password), g_wifi_password);
    copy_string(next_api_token, sizeof(next_api_token), g_api_token);

    if (update->set_wifi) {
        if (!validate_wifi_fields(update->wifi_ssid, update->wifi_password)) {
            return false;
        }
        copy_string(next_snapshot.wifi_ssid, sizeof(next_snapshot.wifi_ssid), update->wifi_ssid);
        copy_string(next_wifi_password, sizeof(next_wifi_password), update->wifi_password);
        if (update->wifi_ssid[0] == '\0') {
            next_wifi_password[0] = '\0';
        }
    }

    if (update->set_network) {
        if (!validate_network_profile_fields(update->timezone_posix,
                                             update->timezone_id,
                                             update->latitude,
                                             update->longitude,
                                             update->location_name)) {
            return false;
        }
        copy_string(next_snapshot.timezone_posix, sizeof(next_snapshot.timezone_posix), update->timezone_posix);
        copy_string(next_snapshot.timezone_id, sizeof(next_snapshot.timezone_id), update->timezone_id);
        copy_string(next_snapshot.location_name, sizeof(next_snapshot.location_name), update->location_name);
        next_snapshot.latitude = update->latitude;
        next_snapshot.longitude = update->longitude;
    }

    if (update->set_api_token) {
        if (!validate_api_token_field(update->api_token)) {
            return false;
        }
        copy_string(next_api_token, sizeof(next_api_token), update->api_token);
    }

    return persist_snapshot(next_snapshot, next_wifi_password, next_api_token);
}

extern "C" bool device_config_save_wifi(const char *ssid, const char *password)
{
    DeviceConfigUpdate update;
    device_config_init();
    if (ssid == nullptr || password == nullptr) {
        return false;
    }

    seed_update_from_current(&update);
    update.set_wifi = true;
    copy_string(update.wifi_ssid, sizeof(update.wifi_ssid), ssid);
    copy_string(update.wifi_password, sizeof(update.wifi_password), password);
    return device_config_apply_update(&update);
}

extern "C" bool device_config_save_network_profile(const char *timezone_posix,
                                                    const char *timezone_id,
                                                    float latitude,
                                                    float longitude,
                                                    const char *location_name)
{
    DeviceConfigUpdate update;
    device_config_init();
    if (timezone_posix == nullptr || timezone_id == nullptr || location_name == nullptr) {
        return false;
    }

    seed_update_from_current(&update);
    update.set_network = true;
    copy_string(update.timezone_posix, sizeof(update.timezone_posix), timezone_posix);
    copy_string(update.timezone_id, sizeof(update.timezone_id), timezone_id);
    copy_string(update.location_name, sizeof(update.location_name), location_name);
    update.latitude = latitude;
    update.longitude = longitude;
    return device_config_apply_update(&update);
}

extern "C" bool device_config_save_api_token(const char *token)
{
    DeviceConfigUpdate update;
    device_config_init();
    if (token == nullptr) {
        return false;
    }

    seed_update_from_current(&update);
    update.set_api_token = true;
    copy_string(update.api_token, sizeof(update.api_token), token);
    return device_config_apply_update(&update);
}

extern "C" bool device_config_get_wifi_password(char *out, uint32_t out_size)
{
    device_config_init();
    if (out == nullptr || out_size == 0U) {
        return false;
    }
    copy_string(out, out_size, g_wifi_password);
    return true;
}

extern "C" bool device_config_get_api_token(char *out, uint32_t out_size)
{
    device_config_init();
    if (out == nullptr || out_size == 0U) {
        return false;
    }
    copy_string(out, out_size, g_api_token);
    return true;
}

extern "C" bool device_config_has_api_token(void)
{
    device_config_init();
    return g_api_token[0] != '\0';
}

extern "C" bool device_config_authenticate_token(const char *token)
{
    device_config_init();
    if (g_api_token[0] == '\0') {
        return true;
    }
    return token != nullptr && constant_time_equals(token, g_api_token);
}

extern "C" bool device_config_restore_defaults(void)
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

extern "C" uint32_t device_config_generation(void)
{
    if (!g_loaded) {
        load_from_preferences();
    }
    return g_generation;
}

extern "C" const char *device_config_backend_name(void)
{
    device_config_init();
    return g_backend_ready ? "NVS_A_B" : "UNAVAILABLE";
}

extern "C" bool device_config_backend_ready(void)
{
    device_config_init();
    return g_backend_ready;
}

extern "C" bool device_config_last_commit_ok(void)
{
    device_config_init();
    return g_last_commit_ok;
}

#include <math.h>
#include <string.h>

extern "C" {
#include "common/crc16.h"
}

#include "services/device_config_internal.hpp"

uint16_t record_crc16(const DeviceConfigStorageRecord &record)
{
    return crc16_buf(reinterpret_cast<const uint8_t *>(&record), offsetof(DeviceConfigStorageRecord, crc16));
}

void fill_record(DeviceConfigStorageRecord *record,
                 const DeviceConfigSnapshot &snapshot,
                 const char *wifi_password,
                 const char *api_token,
                 uint32_t generation)
{
    if (record == nullptr) {
        return;
    }
    memset(record, 0, sizeof(*record));
    record->magic = kRecordMagic;
    record->schema_version = DEVICE_CONFIG_STORAGE_SCHEMA_VERSION;
    record->payload_version = DEVICE_CONFIG_VERSION;
    record->generation = generation;
    copy_string(record->wifi_ssid, sizeof(record->wifi_ssid), snapshot.wifi_ssid);
    copy_string(record->wifi_password, sizeof(record->wifi_password), wifi_password);
    copy_string(record->timezone_posix, sizeof(record->timezone_posix), snapshot.timezone_posix);
    copy_string(record->timezone_id, sizeof(record->timezone_id), snapshot.timezone_id);
    copy_string(record->location_name, sizeof(record->location_name), snapshot.location_name);
    copy_string(record->api_token, sizeof(record->api_token), api_token);
    record->latitude = snapshot.latitude;
    record->longitude = snapshot.longitude;
    record->crc16 = record_crc16(*record);
}

bool record_is_valid(const DeviceConfigStorageRecord &record)
{
    return record.magic == kRecordMagic &&
           record.schema_version == DEVICE_CONFIG_STORAGE_SCHEMA_VERSION &&
           record.payload_version == DEVICE_CONFIG_VERSION &&
           validate_wifi_fields(record.wifi_ssid, record.wifi_password) &&
           validate_network_profile_fields(record.timezone_posix, record.timezone_id, record.latitude, record.longitude, record.location_name) &&
           validate_api_token_field(record.api_token) &&
           record.crc16 == record_crc16(record);
}

void load_runtime_from_record(const DeviceConfigStorageRecord &record, uint8_t slot)
{
    g_snapshot.version = DEVICE_CONFIG_VERSION;
    copy_string(g_snapshot.wifi_ssid, sizeof(g_snapshot.wifi_ssid), record.wifi_ssid);
    copy_string(g_wifi_password, sizeof(g_wifi_password), record.wifi_password);
    copy_string(g_snapshot.timezone_posix, sizeof(g_snapshot.timezone_posix), record.timezone_posix);
    copy_string(g_snapshot.timezone_id, sizeof(g_snapshot.timezone_id), record.timezone_id);
    copy_string(g_snapshot.location_name, sizeof(g_snapshot.location_name), record.location_name);
    copy_string(g_api_token, sizeof(g_api_token), record.api_token);
    g_snapshot.latitude = record.latitude;
    g_snapshot.longitude = record.longitude;
    g_generation = record.generation;
    g_active_slot = slot;
    update_derived_flags();
}

const char *slot_key(uint8_t slot)
{
    return slot == 0U ? kKeyRecordA : kKeyRecordB;
}

bool read_record_locked(uint8_t slot, DeviceConfigStorageRecord *out)
{
    size_t stored_len;
    if (out == nullptr) {
        return false;
    }
    stored_len = g_prefs.getBytesLength(slot_key(slot));
    if (stored_len != sizeof(DeviceConfigStorageRecord)) {
        return false;
    }
    return g_prefs.getBytes(slot_key(slot), out, sizeof(DeviceConfigStorageRecord)) == sizeof(DeviceConfigStorageRecord);
}

void remove_legacy_keys_locked(void)
{
    (void)g_prefs.remove(kKeyWifiSsid);
    (void)g_prefs.remove(kKeyWifiPassword);
    (void)g_prefs.remove(kKeyTimezonePosix);
    (void)g_prefs.remove(kKeyTimezoneId);
    (void)g_prefs.remove(kKeyLatitude);
    (void)g_prefs.remove(kKeyLongitude);
    (void)g_prefs.remove(kKeyLocationName);
    (void)g_prefs.remove(kKeyApiToken);
}

bool persist_record_locked(uint8_t target_slot, const DeviceConfigStorageRecord &record)
{
    if (g_prefs.putBytes(slot_key(target_slot), &record, sizeof(record)) != sizeof(record)) {
        return false;
    }
    if (!g_prefs.putUChar(kKeyActiveSlot, target_slot)) {
        return false;
    }
    g_active_slot = target_slot;
    remove_legacy_keys_locked();
    return true;
}

bool load_from_slots_locked(DeviceConfigStorageRecord *best_record, uint8_t *best_slot)
{
    DeviceConfigStorageRecord record_a = {};
    DeviceConfigStorageRecord record_b = {};
    const bool has_a = read_record_locked(0U, &record_a) && record_is_valid(record_a);
    const bool has_b = read_record_locked(1U, &record_b) && record_is_valid(record_b);

    if (has_a && has_b) {
        if (record_b.generation >= record_a.generation) {
            if (best_record != nullptr) {
                *best_record = record_b;
            }
            if (best_slot != nullptr) {
                *best_slot = 1U;
            }
        } else {
            if (best_record != nullptr) {
                *best_record = record_a;
            }
            if (best_slot != nullptr) {
                *best_slot = 0U;
            }
        }
        return true;
    }
    if (has_a) {
        if (best_record != nullptr) {
            *best_record = record_a;
        }
        if (best_slot != nullptr) {
            *best_slot = 0U;
        }
        return true;
    }
    if (has_b) {
        if (best_record != nullptr) {
            *best_record = record_b;
        }
        if (best_slot != nullptr) {
            *best_slot = 1U;
        }
        return true;
    }
    return false;
}

#include <math.h>
#include <string.h>

extern "C" {
#include "services/device_config_codec.h"
}

namespace {
static float canonical_coordinate(float value)
{
    const float scale = 1000000.0f;
    return roundf(value * scale) / scale;
}

static void trim_string_in_place(char *value)
{
    char *start = value;
    char *end;
    size_t len;
    if (value == nullptr) {
        return;
    }
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        ++start;
    }
    len = strlen(start);
    memmove(value, start, len + 1U);
    end = value + len;
    while (end > value) {
        char ch = *(end - 1);
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            break;
        }
        --end;
    }
    *end = '\0';
}
}

extern "C" void device_config_copy_string(char *dst, size_t dst_size, const char *src)
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

extern "C" bool device_config_string_length_valid(const char *value, size_t max_len)
{
    return value != nullptr && strlen(value) <= max_len;
}

extern "C" bool device_config_float_is_finite(float value)
{
    return isfinite(value) != 0;
}

extern "C" void device_config_canonicalize_update(DeviceConfigUpdate *update)
{
    if (update == nullptr) {
        return;
    }
    if (update->set_network) {
        trim_string_in_place(update->timezone_posix);
        trim_string_in_place(update->timezone_id);
        trim_string_in_place(update->location_name);
        update->latitude = canonical_coordinate(update->latitude);
        update->longitude = canonical_coordinate(update->longitude);
    }
}

extern "C" bool device_config_text_equal_canonical(const char *lhs, const char *rhs)
{
    char lhs_copy[DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN + 1U] = {0};
    char rhs_copy[DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN + 1U] = {0};
    size_t lhs_len;
    size_t rhs_len;

    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    lhs_len = strlen(lhs);
    rhs_len = strlen(rhs);
    if (lhs_len > DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN || rhs_len > DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN) {
        return strcmp(lhs, rhs) == 0;
    }
    device_config_copy_string(lhs_copy, sizeof(lhs_copy), lhs);
    device_config_copy_string(rhs_copy, sizeof(rhs_copy), rhs);
    trim_string_in_place(lhs_copy);
    trim_string_in_place(rhs_copy);
    return strcmp(lhs_copy, rhs_copy) == 0;
}

extern "C" bool device_config_coordinate_equal(float lhs, float rhs)
{
    return fabsf(canonical_coordinate(lhs) - canonical_coordinate(rhs)) <= 0.0000005f;
}

extern "C" bool device_config_validate_wifi_fields(const char *ssid, const char *password)
{
    size_t password_len;
    if (ssid == nullptr || password == nullptr) {
        return false;
    }
    if (!device_config_string_length_valid(ssid, DEVICE_CONFIG_WIFI_SSID_MAX_LEN) ||
        !device_config_string_length_valid(password, DEVICE_CONFIG_WIFI_PASSWORD_MAX_LEN)) {
        return false;
    }
    password_len = strlen(password);
    if (ssid[0] == '\0') {
        return password_len == 0U;
    }
    return password_len >= 8U || password_len == 0U;
}

extern "C" bool device_config_validate_network_profile_fields(const char *timezone_posix,
                                                                const char *timezone_id,
                                                                float latitude,
                                                                float longitude,
                                                                const char *location_name)
{
    if (timezone_posix == nullptr || timezone_id == nullptr || location_name == nullptr) {
        return false;
    }
    if (!device_config_string_length_valid(timezone_posix, DEVICE_CONFIG_TIMEZONE_POSIX_MAX_LEN) ||
        !device_config_string_length_valid(timezone_id, DEVICE_CONFIG_TIMEZONE_ID_MAX_LEN) ||
        !device_config_string_length_valid(location_name, DEVICE_CONFIG_LOCATION_NAME_MAX_LEN)) {
        return false;
    }
    if (timezone_posix[0] == '\0' || timezone_id[0] == '\0' || location_name[0] == '\0') {
        return false;
    }
    if (!device_config_float_is_finite(latitude) || !device_config_float_is_finite(longitude) ||
        latitude < -90.0f || latitude > 90.0f || longitude < -180.0f || longitude > 180.0f) {
        return false;
    }
    return true;
}

extern "C" bool device_config_validate_api_token_field(const char *token)
{
    return token != nullptr && device_config_string_length_valid(token, DEVICE_CONFIG_API_TOKEN_MAX_LEN);
}

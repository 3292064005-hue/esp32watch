#include <string.h>

extern "C" {
#include "services/device_config.h"
}

#include "services/device_config_internal.hpp"
#include "services/device_config_authority.h"

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
    DeviceConfigAuthorityApplyReport apply = {};
    return device_config_authority_apply_update(&update, &apply);
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
    DeviceConfigAuthorityApplyReport apply = {};
    return device_config_authority_apply_update(&update, &apply);
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
    DeviceConfigAuthorityApplyReport apply = {};
    return device_config_authority_apply_update(&update, &apply);
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
    DeviceConfigAuthorityApplyReport apply = {};
    device_config_init();
    return device_config_authority_restore_defaults(&apply);
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

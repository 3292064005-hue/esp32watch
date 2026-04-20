#ifndef SERVICES_DEVICE_CONFIG_CODEC_H
#define SERVICES_DEVICE_CONFIG_CODEC_H

#include "services/device_config.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void device_config_copy_string(char *dst, size_t dst_size, const char *src);
bool device_config_string_length_valid(const char *value, size_t max_len);
bool device_config_float_is_finite(float value);
void device_config_canonicalize_update(DeviceConfigUpdate *update);
bool device_config_text_equal_canonical(const char *lhs, const char *rhs);
bool device_config_coordinate_equal(float lhs, float rhs);
bool device_config_validate_wifi_fields(const char *ssid, const char *password);
bool device_config_validate_network_profile_fields(const char *timezone_posix,
                                                   const char *timezone_id,
                                                   float latitude,
                                                   float longitude,
                                                   const char *location_name);
bool device_config_validate_api_token_field(const char *token);

#ifdef __cplusplus
}
#endif

#endif

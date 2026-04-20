#ifndef NETWORK_SYNC_CODEC_H
#define NETWORK_SYNC_CODEC_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "services/device_config.h"
#ifdef __cplusplus
}

bool network_sync_parse_weather_payload(const String &payload, float *out_temp_c, uint8_t *out_weather_code);
void network_sync_append_url_encoded(String &dst, const char *src);
String network_sync_build_weather_url(const DeviceConfigSnapshot &cfg, const char *base_url);

#endif

#endif

#pragma once
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void web_json_escape_append(String &out, const char *src);
void web_json_kv_str(String &out, const char *key, const char *value, bool last);
void web_json_kv_bool(String &out, const char *key, bool value, bool last);
void web_json_kv_u32(String &out, const char *key, uint32_t value, bool last);
void web_json_kv_i32(String &out, const char *key, int32_t value, bool last);
void web_json_kv_f32(String &out, const char *key, float value, uint8_t digits, bool last);

#ifdef __cplusplus
}
#endif

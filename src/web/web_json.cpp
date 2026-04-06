#include "web/web_json.h"
#include <stdio.h>
#include <string.h>

void web_json_escape_append(String &out, const char *src)
{
    if (!src) {
        return;
    }

    for (const char *p = src; *p; ++p) {
        char c = *p;
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
    }
}

void web_json_kv_str(String &out, const char *key, const char *value, bool last)
{
    out += "\"";
    out += key;
    out += "\":\"";
    web_json_escape_append(out, value);
    out += "\"";
    if (!last) {
        out += ",";
    }
}

void web_json_kv_bool(String &out, const char *key, bool value, bool last)
{
    out += "\"";
    out += key;
    out += "\":";
    out += value ? "true" : "false";
    if (!last) {
        out += ",";
    }
}

void web_json_kv_u32(String &out, const char *key, uint32_t value, bool last)
{
    out += "\"";
    out += key;
    out += "\":";
    out += value;
    if (!last) {
        out += ",";
    }
}

void web_json_kv_i32(String &out, const char *key, int32_t value, bool last)
{
    out += "\"";
    out += key;
    out += "\":";
    out += value;
    if (!last) {
        out += ",";
    }
}

void web_json_kv_f32(String &out, const char *key, float value, uint8_t digits, bool last)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", digits, value);

    out += "\"";
    out += key;
    out += "\":";
    out += buf;
    if (!last) {
        out += ",";
    }
}

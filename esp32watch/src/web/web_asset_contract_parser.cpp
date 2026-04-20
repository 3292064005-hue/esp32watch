#include "web/web_asset_contract_parser.h"
#include <ArduinoJson.h>
#include <string.h>

static bool web_contract_find_string_field(const String &json,
                                           const char *key,
                                           char *out_value,
                                           size_t out_size)
{
    int key_index;
    int value_index;
    int end_index;

    if (key == nullptr || out_value == nullptr || out_size == 0U) {
        return false;
    }

    key_index = json.indexOf(key);
    if (key_index < 0) {
        return false;
    }
    value_index = json.indexOf(':', key_index);
    if (value_index < 0) {
        return false;
    }
    value_index = json.indexOf('"', value_index);
    if (value_index < 0) {
        return false;
    }
    end_index = json.indexOf('"', value_index + 1);
    if (end_index < 0 || end_index <= value_index) {
        return false;
    }
    ++value_index;
    String value = json.substring(value_index, end_index);
    strncpy(out_value, value.c_str(), out_size - 1U);
    out_value[out_size - 1U] = '\0';
    return true;
}

static uint32_t web_parse_asset_contract_version(const String &json)
{
    const char *key = "\"assetContractVersion\"";
    int key_index = json.indexOf(key);
    int value_index;

    if (key_index < 0) {
        return 0U;
    }
    value_index = json.indexOf(':', key_index);
    if (value_index < 0) {
        return 0U;
    }
    while (++value_index < (int)json.length()) {
        char ch = json[value_index];
        if (ch >= '0' && ch <= '9') {
            break;
        }
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            return 0U;
        }
    }
    return (uint32_t)json.substring(value_index, (int)json.length()).toInt();
}

bool web_asset_contract_find_hash_compat(const String &json, const char *entry_name, char out_hash[9])
{
    String name_needle;
    int entry_index;
    int hash_index;
    int value_index;

    if (entry_name == nullptr || out_hash == nullptr) {
        return false;
    }

    name_needle = "\"";
    name_needle += entry_name;
    name_needle += "\"";
    entry_index = json.indexOf(name_needle.c_str());
    if (entry_index < 0) {
        return false;
    }
    hash_index = json.indexOf("\"fnv1a32\"", entry_index);
    if (hash_index < 0) {
        return false;
    }
    value_index = json.indexOf(':', hash_index);
    if (value_index < 0) {
        return false;
    }
    value_index = json.indexOf('"', value_index);
    if (value_index < 0 || (value_index + 8) >= (int)json.length()) {
        return false;
    }
    ++value_index;
    for (int i = 0; i < 8; ++i) {
        char ch = json[value_index + i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))) {
            return false;
        }
        out_hash[i] = ch;
    }
    out_hash[8] = '\0';
    return true;
}

bool web_asset_contract_parse_metadata(const String &json, WebAssetContractMetadata *out)
{
    StaticJsonDocument<16384> doc;
    DeserializationError error;

    if (out == nullptr) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->generated_at, "UNKNOWN", sizeof(out->generated_at) - 1U);
    out->generated_at[sizeof(out->generated_at) - 1U] = '\0';

    error = deserializeJson(doc, json.c_str());
    if (!error) {
        const char *generated_at = doc["generatedAtUtc"].as<const char *>();
        out->parsed_with_json = true;
        out->version = doc["assetContractVersion"].as<uint32_t>();
        if (generated_at != nullptr && generated_at[0] != '\0') {
            strncpy(out->generated_at, generated_at, sizeof(out->generated_at) - 1U);
            out->generated_at[sizeof(out->generated_at) - 1U] = '\0';
        }
        if (out->version != 0U) {
            return true;
        }
    }

    out->parsed_with_json = false;
    out->version = web_parse_asset_contract_version(json);
    (void)web_contract_find_string_field(json,
                                         "\"generatedAtUtc\"",
                                         out->generated_at,
                                         sizeof(out->generated_at));
    return out->version != 0U;
}

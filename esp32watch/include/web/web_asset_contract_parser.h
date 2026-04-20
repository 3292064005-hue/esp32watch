#ifndef WEB_ASSET_CONTRACT_PARSER_H
#define WEB_ASSET_CONTRACT_PARSER_H

#include <Arduino.h>
#include <stdint.h>

typedef struct {
    uint32_t version;
    bool parsed_with_json;
    char generated_at[32];
} WebAssetContractMetadata;

/**
 * @brief Parse asset-contract metadata using the JSON schema path first and a
 *        compatibility fallback for malformed-but-recoverable payloads.
 *
 * @param[in] json Raw asset-contract payload.
 * @param[out] out Parsed metadata destination.
 * @return true when version/generatedAt metadata could be recovered.
 */
bool web_asset_contract_parse_metadata(const String &json, WebAssetContractMetadata *out);

/**
 * @brief Find a required file hash through the compatibility string parser.
 *
 * @param[in] json Raw asset-contract payload.
 * @param[in] entry_name Required file entry name.
 * @param[out] out_hash Destination for the 8-character FNV1A hash plus terminator.
 * @return true when a valid 8-character uppercase hash was found.
 */
bool web_asset_contract_find_hash_compat(const String &json, const char *entry_name, char out_hash[9]);

#endif

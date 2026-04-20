#include <cassert>
#include <cstdio>
#include <cstring>

#include "web/web_asset_contract_parser.h"

static String valid_contract()
{
    return String(
        "{\n"
        "  \"assetContractVersion\": 6,\n"
        "  \"generatedAtUtc\": \"2026-04-19T00:00:00Z\",\n"
        "  \"files\": {\n"
        "    \"index.html\": {\"fnv1a32\": \"A1B2C3D4\"}\n"
        "  }\n"
        "}\n");
}

static String compat_contract()
{
    return String(
        "prefix \"assetContractVersion\": 6, broken-json, \"generatedAtUtc\": \"2026-04-19T00:00:00Z\", "
        "\"files\": { \"index.html\": { \"fnv1a32\": \"A1B2C3D4\" } }");
}

static String invalid_contract()
{
    return String(
        "{\n"
        "  \"assetContractVersion\": 6,\n"
        "  \"generatedAtUtc\": \"2026-04-19T00:00:00Z\",\n"
        "  \"files\": {\n"
        "    \"index.html\": {\"fnv1a32\": \"SHORT\"}\n"
        "  }\n"
        "}\n");
}

int main()
{
    WebAssetContractMetadata meta = {};
    char hash[9] = {0};

    assert(web_asset_contract_parse_metadata(valid_contract(), &meta));
    assert(meta.version == 6U);
    assert(std::strcmp(meta.generated_at, "2026-04-19T00:00:00Z") == 0);
    assert(web_asset_contract_find_hash_compat(valid_contract(), "index.html", hash));
    assert(std::strcmp(hash, "A1B2C3D4") == 0);

    std::memset(&meta, 0, sizeof(meta));
    std::memset(hash, 0, sizeof(hash));
    assert(web_asset_contract_parse_metadata(compat_contract(), &meta));
    assert(!meta.parsed_with_json);
    assert(meta.version == 6U);
    assert(std::strcmp(meta.generated_at, "2026-04-19T00:00:00Z") == 0);
    assert(web_asset_contract_find_hash_compat(compat_contract(), "index.html", hash));
    assert(std::strcmp(hash, "A1B2C3D4") == 0);

    std::memset(hash, 0, sizeof(hash));
    assert(!web_asset_contract_find_hash_compat(invalid_contract(), "index.html", hash));

    std::puts("[OK] web asset contract parser compatibility/malformed checks passed");
    return 0;
}

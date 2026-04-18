#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the web server, route table, and filesystem-backed UI assets.
 *
 * @return true when the server started; false when startup could not complete.
 * @throws None.
 * @boundary_behavior Filesystem mount failure is reported through status helpers; the runtime may still expose fallback routes for diagnostics.
 */
bool web_server_init(void);

/**
 * @brief Advance periodic web-server maintenance hooks.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return void
 * @throws None.
 */
void web_server_poll(uint32_t now_ms);

/**
 * @brief Report whether the web control plane still has queued work that should block deep idle.
 *
 * @return true when tracked web actions are still queued for execution.
 * @throws None.
 */
bool web_server_has_pending_actions(void);

/**
 * @brief Report whether the minimal web control plane is available.
 *
 * @return true when the HTTP server and mutation/status routes are initialized, even if Wi-Fi is still connecting or rich static assets are unavailable.
 * @throws None.
 */
bool web_server_control_plane_ready(void);

/**
 * @brief Report whether the rich web console assets are available.
 *
 * @return true when the control plane is ready and the LittleFS-backed UI assets passed validation.
 * @throws None.
 */
bool web_server_console_ready(void);

/**
 * @brief Report whether the HTTP server stack is ready to accept requests.
 *
 * @return true when the control plane routes were initialized successfully.
 * @throws None.
 */
bool web_server_is_ready(void);

/**
 * @brief Report whether LittleFS mounted successfully.
 *
 * @return true when the filesystem is mounted and readable; false otherwise.
 * @throws None.
 */
bool web_server_filesystem_ready(void);

/**
 * @brief Report whether required UI assets are present on the mounted filesystem.
 *
 * @return true when the expected UI entry assets are available; false otherwise.
 * @throws None.
 */
bool web_server_filesystem_assets_ready(void);

/**
 * @brief Report whether the LittleFS asset contract file passed validation.
 *
 * @return true when the runtime parsed the asset contract, the version matched, and the required entry assets were declared.
 * @throws None.
 */
bool web_server_asset_contract_ready(void);

/**
 * @brief Report whether the mounted web assets matched the manifest hashes.
 *
 * @return true when every required asset matched the manifest hash recorded in /asset-contract.json.
 * @throws None.
 */
bool web_server_asset_contract_hash_verified(void);

/**
 * @brief Return the firmware-side web asset contract version.
 *
 * @return Monotonic asset contract version expected by this firmware build.
 * @throws None.
 */
uint32_t web_server_asset_contract_version(void);

/**
 * @brief Return the FNV1a32 hash of the loaded asset-contract manifest JSON.
 *
 * @return Non-zero hash when /asset-contract.json was read successfully; 0 otherwise.
 * @throws None.
 */
uint32_t web_server_asset_contract_hash(void);

/**
 * @brief Return the UTC timestamp embedded in the loaded asset contract.
 *
 * @return ISO-8601 UTC timestamp string when the asset contract declared one; "UNKNOWN" otherwise.
 * @throws None.
 */
const char *web_server_asset_contract_generated_at(void);

/**
 * @brief Return the expected hash recorded for a specific asset file.
 *
 * @param[in] asset_name Asset file name such as "app.js" or "contract-bootstrap.json".
 * @return Uppercase 8-character FNV1a hash when known; "UNKNOWN" when absent.
 * @throws None.
 */
const char *web_server_asset_expected_hash(const char *asset_name);

/**
 * @brief Return the measured FNV1a hash for a specific mounted asset file.
 *
 * @param[in] asset_name Asset file name such as "app.js" or "contract-bootstrap.json".
 * @return Uppercase 8-character FNV1a hash when known; "UNKNOWN" when absent.
 * @throws None.
 */
const char *web_server_asset_actual_hash(const char *asset_name);

/**
 * @brief Return the current filesystem status label used by diagnostics and web APIs.
 *
 * @return Stable status string such as READY, ASSET_MISSING, MOUNT_FAILED, or RECOVERED.
 * @throws None.
 */
const char *web_server_filesystem_status(void);

#ifdef __cplusplus
}
#endif

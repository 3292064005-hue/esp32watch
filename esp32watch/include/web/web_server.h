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
 * @brief Report whether the HTTP server stack is ready to accept requests.
 *
 * @return true when the server was initialized successfully.
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
 * @brief Return the current filesystem status label used by diagnostics and web APIs.
 *
 * @return Stable status string such as READY, ASSET_MISSING, MOUNT_FAILED, or RECOVERED.
 * @throws None.
 */
const char *web_server_filesystem_status(void);

#ifdef __cplusplus
}
#endif

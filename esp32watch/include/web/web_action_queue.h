#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "key.h"
#include "app_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WEB_ACTION_NONE = 0,
    WEB_ACTION_KEY,
    WEB_ACTION_COMMAND,
    WEB_ACTION_OVERLAY_TEXT,
    WEB_ACTION_OVERLAY_CLEAR
} WebActionType;

typedef enum {
    WEB_ACTION_STATUS_NONE = 0,
    WEB_ACTION_STATUS_ACCEPTED,
    WEB_ACTION_STATUS_RUNNING,
    WEB_ACTION_STATUS_APPLIED,
    WEB_ACTION_STATUS_REJECTED,
    WEB_ACTION_STATUS_FAILED
} WebActionStatus;

typedef struct {
    uint32_t id;
    WebActionType type;
    union {
        struct {
            KeyId id;
            KeyEventType event_type;
        } key;
        AppCommand command;
        struct {
            char text[64];
            uint32_t duration_ms;
        } overlay;
    } data;
} WebAction;

typedef struct {
    uint32_t id;
    WebActionType type;
    WebActionStatus status;
    AppCommandType command_type;
    bool command_ok;
    AppCommandResultCode command_code;
    uint32_t accepted_at_ms;
    uint32_t started_at_ms;
    uint32_t completed_at_ms;
    char message[32];
} WebActionStatusSnapshot;

/**
 * @brief Enqueue a web-originated mutation for main-loop execution and create a status record.
 *
 * @param[in] action Action payload. Must not be NULL.
 * @param[in] accepted_at_ms Monotonic timestamp captured by the HTTP callback.
 * @param[out] out_id Optional accepted action id.
 * @return true when the action was queued; false for NULL input or full queue.
 * @throws None.
 * @boundary_behavior Thread-safe for AsyncWebServer producers and main-loop consumers.
 */
bool web_action_enqueue_tracked(const WebAction *action, uint32_t accepted_at_ms, uint32_t *out_id);

/**
 * @brief Dequeue the next accepted web action for main-loop execution.
 *
 * @param[out] out Destination action buffer.
 * @return true when an action was copied; false when the queue is empty or out is NULL.
 * @throws None.
 * @boundary_behavior Thread-safe single-consumer API; copied action remains valid after unlock.
 */
bool web_action_dequeue(WebAction *out);

uint16_t web_action_queue_depth(void);
uint16_t web_action_queue_drop_count(void);
void web_action_mark_running(uint32_t action_id, uint32_t started_at_ms);
void web_action_mark_completed(uint32_t action_id,
                               WebActionStatus final_status,
                               bool command_ok,
                               AppCommandResultCode command_code,
                               uint32_t completed_at_ms,
                               const char *message);
bool web_action_status_lookup(uint32_t action_id, WebActionStatusSnapshot *out);
bool web_action_status_latest(WebActionStatusSnapshot *out);
const char *web_action_type_name(WebActionType type);
const char *web_action_status_name(WebActionStatus status);

#ifdef __cplusplus
}
#endif

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

typedef struct {
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

bool web_action_enqueue(const WebAction *action);
bool web_action_dequeue(WebAction *out);
uint16_t web_action_queue_depth(void);
uint16_t web_action_queue_drop_count(void);

#ifdef __cplusplus
}
#endif

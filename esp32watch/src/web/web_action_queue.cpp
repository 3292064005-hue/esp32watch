#include "web/web_action_queue.h"
#include <string.h>

#define WEB_ACTION_QUEUE_SIZE 32U
#define WEB_ACTION_STATUS_HISTORY_SIZE 64U

typedef struct {
    WebAction items[WEB_ACTION_QUEUE_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t drop_count;
    uint32_t next_action_id;
    WebActionStatusSnapshot history[WEB_ACTION_STATUS_HISTORY_SIZE];
    uint16_t history_head;
    uint16_t history_count;
} WebActionQueueState;

static WebActionQueueState g_queue = {
    .head = 0,
    .tail = 0,
    .drop_count = 0,
    .next_action_id = 1U,
    .history_head = 0,
    .history_count = 0,
};

static uint16_t web_action_history_index_for_slot(uint16_t slot)
{
    return (uint16_t)((g_queue.history_head + slot) % WEB_ACTION_STATUS_HISTORY_SIZE);
}

static WebActionStatusSnapshot *web_action_find_history(uint32_t action_id)
{
    uint16_t i;
    for (i = 0; i < g_queue.history_count; ++i) {
        WebActionStatusSnapshot *entry = &g_queue.history[web_action_history_index_for_slot(i)];
        if (entry->id == action_id) {
            return entry;
        }
    }
    return NULL;
}

static WebActionStatusSnapshot *web_action_reserve_history_slot(void)
{
    uint16_t slot;
    if (g_queue.history_count < WEB_ACTION_STATUS_HISTORY_SIZE) {
        slot = web_action_history_index_for_slot(g_queue.history_count);
        g_queue.history_count++;
    } else {
        slot = g_queue.history_head;
        g_queue.history_head = (uint16_t)((g_queue.history_head + 1U) % WEB_ACTION_STATUS_HISTORY_SIZE);
    }
    memset(&g_queue.history[slot], 0, sizeof(g_queue.history[slot]));
    return &g_queue.history[slot];
}

bool web_action_enqueue_tracked(const WebAction *action, uint32_t accepted_at_ms, uint32_t *out_id)
{
    WebAction queued = {0};
    WebActionStatusSnapshot *history_entry;
    uint16_t next_tail;

    if (action == NULL) {
        return false;
    }

    next_tail = (uint16_t)((g_queue.tail + 1U) % WEB_ACTION_QUEUE_SIZE);
    if (next_tail == g_queue.head) {
        g_queue.drop_count++;
        return false;
    }

    queued = *action;
    queued.id = g_queue.next_action_id++;
    if (queued.id == 0U) {
        queued.id = g_queue.next_action_id++;
    }

    history_entry = web_action_reserve_history_slot();
    history_entry->id = queued.id;
    history_entry->type = queued.type;
    history_entry->status = WEB_ACTION_STATUS_ACCEPTED;
    history_entry->command_type = queued.type == WEB_ACTION_COMMAND ? queued.data.command.type : APP_COMMAND_NONE;
    history_entry->command_ok = false;
    history_entry->command_code = APP_COMMAND_RESULT_UNSUPPORTED;
    history_entry->accepted_at_ms = accepted_at_ms;
    strncpy(history_entry->message, "accepted", sizeof(history_entry->message) - 1U);

    memcpy(&g_queue.items[g_queue.tail], &queued, sizeof(queued));
    g_queue.tail = next_tail;

    if (out_id != NULL) {
        *out_id = queued.id;
    }
    return true;
}

bool web_action_dequeue(WebAction *out)
{
    if (!out) {
        return false;
    }

    if (g_queue.head == g_queue.tail) {
        return false;
    }

    memcpy(out, &g_queue.items[g_queue.head], sizeof(WebAction));
    g_queue.head = (uint16_t)((g_queue.head + 1U) % WEB_ACTION_QUEUE_SIZE);

    return true;
}

uint16_t web_action_queue_depth(void)
{
    if (g_queue.tail >= g_queue.head) {
        return (uint16_t)(g_queue.tail - g_queue.head);
    }
    return (uint16_t)(WEB_ACTION_QUEUE_SIZE - (g_queue.head - g_queue.tail));
}

uint16_t web_action_queue_drop_count(void)
{
    return g_queue.drop_count;
}

void web_action_mark_running(uint32_t action_id, uint32_t started_at_ms)
{
    WebActionStatusSnapshot *entry = web_action_find_history(action_id);
    if (entry == NULL) {
        return;
    }
    entry->status = WEB_ACTION_STATUS_RUNNING;
    entry->started_at_ms = started_at_ms;
    strncpy(entry->message, "running", sizeof(entry->message) - 1U);
}

void web_action_mark_completed(uint32_t action_id,
                               WebActionStatus final_status,
                               bool command_ok,
                               AppCommandResultCode command_code,
                               uint32_t completed_at_ms,
                               const char *message)
{
    WebActionStatusSnapshot *entry = web_action_find_history(action_id);
    if (entry == NULL) {
        return;
    }
    entry->status = final_status;
    entry->command_ok = command_ok;
    entry->command_code = command_code;
    entry->completed_at_ms = completed_at_ms;
    if (message != NULL) {
        strncpy(entry->message, message, sizeof(entry->message) - 1U);
        entry->message[sizeof(entry->message) - 1U] = '\0';
    }
}

bool web_action_status_lookup(uint32_t action_id, WebActionStatusSnapshot *out)
{
    const WebActionStatusSnapshot *entry;
    if (out == NULL || action_id == 0U) {
        return false;
    }
    entry = web_action_find_history(action_id);
    if (entry == NULL) {
        return false;
    }
    *out = *entry;
    return true;
}

bool web_action_status_latest(WebActionStatusSnapshot *out)
{
    uint16_t slot;
    if (out == NULL || g_queue.history_count == 0U) {
        return false;
    }
    slot = web_action_history_index_for_slot((uint16_t)(g_queue.history_count - 1U));
    *out = g_queue.history[slot];
    return true;
}

const char *web_action_type_name(WebActionType type)
{
    switch (type) {
        case WEB_ACTION_KEY: return "KEY";
        case WEB_ACTION_COMMAND: return "COMMAND";
        case WEB_ACTION_OVERLAY_TEXT: return "OVERLAY_TEXT";
        case WEB_ACTION_OVERLAY_CLEAR: return "OVERLAY_CLEAR";
        default: return "NONE";
    }
}

const char *web_action_status_name(WebActionStatus status)
{
    switch (status) {
        case WEB_ACTION_STATUS_ACCEPTED: return "ACCEPTED";
        case WEB_ACTION_STATUS_RUNNING: return "RUNNING";
        case WEB_ACTION_STATUS_APPLIED: return "APPLIED";
        case WEB_ACTION_STATUS_REJECTED: return "REJECTED";
        case WEB_ACTION_STATUS_FAILED: return "FAILED";
        default: return "NONE";
    }
}

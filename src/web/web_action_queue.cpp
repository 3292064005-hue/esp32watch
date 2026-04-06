#include "web/web_action_queue.h"
#include <string.h>

#define WEB_ACTION_QUEUE_SIZE 32

typedef struct {
    WebAction items[WEB_ACTION_QUEUE_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t drop_count;
} WebActionQueueState;

static WebActionQueueState g_queue = {
    .head = 0,
    .tail = 0,
    .drop_count = 0
};

bool web_action_enqueue(const WebAction *action)
{
    if (!action) {
        return false;
    }

    uint16_t next_tail = (g_queue.tail + 1) % WEB_ACTION_QUEUE_SIZE;

    if (next_tail == g_queue.head) {
        g_queue.drop_count++;
        return false;
    }

    memcpy(&g_queue.items[g_queue.tail], action, sizeof(WebAction));
    g_queue.tail = next_tail;

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
    g_queue.head = (g_queue.head + 1) % WEB_ACTION_QUEUE_SIZE;

    return true;
}

uint16_t web_action_queue_depth(void)
{
    if (g_queue.tail >= g_queue.head) {
        return g_queue.tail - g_queue.head;
    } else {
        return WEB_ACTION_QUEUE_SIZE - (g_queue.head - g_queue.tail);
    }
}

uint16_t web_action_queue_drop_count(void)
{
    return g_queue.drop_count;
}

#include "key.h"
#include "app_config.h"
#include "platform_api.h"
#include "common/ringbuf.h"

#define KEY_QUEUE_SIZE      16
#define KEY_DEBOUNCE_TICKS  3
#define KEY_LONG_TICKS      50
#define KEY_REPEAT_START    70
#define KEY_REPEAT_RATE     12

typedef struct {
    PlatformGpioPort *port;
    uint16_t pin;
    bool active_low;
    bool stable_down;
    bool last_sample;
    uint8_t debounce;
    uint16_t hold_ticks;
    bool long_sent;
} KeyState;

static KeyState g_keys[KEY_ID_COUNT];
static KeyEvent g_queue_storage[KEY_QUEUE_SIZE];
static RingBuf g_queue;

static void key_push_event(KeyId id, KeyEventType type)
{
    KeyEvent event;
    event.id = id;
    event.type = type;
    (void)ringbuf_push(&g_queue, &event);
}

static bool read_physical(KeyState *key)
{
    PlatformPinState state = platform_gpio_read(key->port, key->pin);
    bool raw = (state == PLATFORM_PIN_HIGH);
    return key->active_low ? !raw : raw;
}

void key_init(void)
{
    g_keys[KEY_ID_UP]   = (KeyState){KEY_UP_GPIO_Port, KEY_UP_Pin, true, false, false, 0, 0, false};
    g_keys[KEY_ID_DOWN] = (KeyState){KEY_DOWN_GPIO_Port, KEY_DOWN_Pin, true, false, false, 0, 0, false};
    g_keys[KEY_ID_OK]   = (KeyState){KEY_OK_GPIO_Port, KEY_OK_Pin, true, false, false, 0, 0, false};
    g_keys[KEY_ID_BACK] = (KeyState){KEY_BACK_GPIO_Port, KEY_BACK_Pin, true, false, false, 0, 0, false};
    ringbuf_init(&g_queue, g_queue_storage, KEY_QUEUE_SIZE, sizeof(KeyEvent));
}

void key_scan_10ms(void)
{
    for (uint8_t i = 0; i < KEY_ID_COUNT; ++i) {
        KeyState *k = &g_keys[i];
        bool sample = read_physical(k);

        if (sample == k->last_sample) {
            if (k->debounce < KEY_DEBOUNCE_TICKS) {
                k->debounce++;
            }
        } else {
            k->debounce = 0;
            k->last_sample = sample;
        }

        if (k->debounce >= KEY_DEBOUNCE_TICKS && sample != k->stable_down) {
            k->stable_down = sample;
            if (sample) {
                k->hold_ticks = 0;
                k->long_sent = false;
                key_push_event((KeyId)i, KEY_EVENT_PRESS);
            } else {
                if (!k->long_sent) {
                    key_push_event((KeyId)i, KEY_EVENT_SHORT);
                }
                key_push_event((KeyId)i, KEY_EVENT_RELEASE);
            }
        }

        if (k->stable_down) {
            k->hold_ticks++;
            if (!k->long_sent && k->hold_ticks >= KEY_LONG_TICKS) {
                k->long_sent = true;
                key_push_event((KeyId)i, KEY_EVENT_LONG);
            } else if (k->long_sent && k->hold_ticks >= KEY_REPEAT_START) {
                if (((k->hold_ticks - KEY_REPEAT_START) % KEY_REPEAT_RATE) == 0U) {
                    key_push_event((KeyId)i, KEY_EVENT_REPEAT);
                }
            }
        }
    }
}

bool key_pop_event(KeyEvent *event)
{
    return ringbuf_pop(&g_queue, event);
}

bool key_is_down(KeyId id)
{
    return g_keys[id].stable_down;
}

uint16_t key_get_overflow_count(void)
{
    uint32_t n = ringbuf_get_overflow_count(&g_queue);
    return (n > 0xFFFFU) ? 0xFFFFU : (uint16_t)n;
}

void key_inject_event(KeyId id, KeyEventType type)
{
    key_push_event(id, type);
}



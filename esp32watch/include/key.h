#ifndef KEY_H
#define KEY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_ID_UP = 0,
    KEY_ID_DOWN,
    KEY_ID_OK,
    KEY_ID_BACK,
    KEY_ID_COUNT
} KeyId;

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,
    KEY_EVENT_SHORT,
    KEY_EVENT_LONG,
    KEY_EVENT_REPEAT,
    KEY_EVENT_RELEASE
} KeyEventType;

typedef struct {
    KeyId id;
    KeyEventType type;
} KeyEvent;

void key_init(void);
void key_scan_10ms(void);
bool key_pop_event(KeyEvent *event);
bool key_is_down(KeyId id);
uint16_t key_get_overflow_count(void);
void key_inject_event(KeyId id, KeyEventType type);

#ifdef __cplusplus
}
#endif

#endif

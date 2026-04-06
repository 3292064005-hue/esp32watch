#include "common/ringbuf.h"
#include <string.h>

void ringbuf_init(RingBuf *rb, void *storage, uint16_t capacity, uint16_t elem_size)
{
    if (rb == NULL) {
        return;
    }
    rb->buffer = (uint8_t *)storage;
    rb->capacity = capacity;
    rb->elem_size = elem_size;
    ringbuf_reset(rb);
}

void ringbuf_reset(RingBuf *rb)
{
    if (rb == NULL) {
        return;
    }
    rb->head = 0U;
    rb->tail = 0U;
    rb->count = 0U;
    rb->overflow_count = 0U;
}

bool ringbuf_push(RingBuf *rb, const void *elem)
{
    uint16_t offset;
    if (rb == NULL || elem == NULL || rb->buffer == NULL || rb->capacity == 0U || rb->elem_size == 0U) {
        return false;
    }
    if (rb->count >= rb->capacity) {
        rb->overflow_count++;
        return false;
    }
    offset = (uint16_t)(rb->head * rb->elem_size);
    memcpy(&rb->buffer[offset], elem, rb->elem_size);
    rb->head = (uint16_t)((rb->head + 1U) % rb->capacity);
    rb->count++;
    return true;
}

bool ringbuf_pop(RingBuf *rb, void *elem)
{
    uint16_t offset;
    if (rb == NULL || elem == NULL || rb->buffer == NULL || rb->capacity == 0U || rb->elem_size == 0U) {
        return false;
    }
    if (rb->count == 0U) {
        return false;
    }
    offset = (uint16_t)(rb->tail * rb->elem_size);
    memcpy(elem, &rb->buffer[offset], rb->elem_size);
    rb->tail = (uint16_t)((rb->tail + 1U) % rb->capacity);
    rb->count--;
    return true;
}

bool ringbuf_is_empty(const RingBuf *rb)
{
    return rb == NULL || rb->count == 0U;
}

uint16_t ringbuf_count(const RingBuf *rb)
{
    return rb == NULL ? 0U : rb->count;
}

uint32_t ringbuf_get_overflow_count(const RingBuf *rb)
{
    return rb == NULL ? 0U : rb->overflow_count;
}

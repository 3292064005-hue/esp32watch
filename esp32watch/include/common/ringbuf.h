#ifndef COMMON_RINGBUF_H
#define COMMON_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buffer;
    uint16_t capacity;
    uint16_t elem_size;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint32_t overflow_count;
} RingBuf;

void ringbuf_init(RingBuf *rb, void *storage, uint16_t capacity, uint16_t elem_size);
void ringbuf_reset(RingBuf *rb);
bool ringbuf_push(RingBuf *rb, const void *elem);
bool ringbuf_pop(RingBuf *rb, void *elem);
bool ringbuf_is_empty(const RingBuf *rb);
uint16_t ringbuf_count(const RingBuf *rb);
uint32_t ringbuf_get_overflow_count(const RingBuf *rb);

#endif

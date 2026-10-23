/**
 * @file    ringbuf.c
 * @brief   Byte ring buffer implementation (see ringbuf.h).
 */
#include "ringbuf.h"
#include "bits.h"

bool ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint16_t size)
{
    if ((rb == NULL) || (storage == NULL) || (size < 2U) || !IS_POW2(size)) {
        return false;
    }
    rb->buffer = storage;
    rb->mask   = (uint16_t)(size - 1U);
    rb->head   = 0U;
    rb->tail   = 0U;
    return true;
}

void ringbuf_reset(ringbuf_t *rb)
{
    rb->tail = rb->head;
}

uint16_t ringbuf_count(const ringbuf_t *rb)
{
    /* unsigned subtraction handles the wrap of the free-running indices */
    return (uint16_t)(rb->head - rb->tail);
}

uint16_t ringbuf_capacity(const ringbuf_t *rb)
{
    return (uint16_t)(rb->mask + 1U);
}

uint16_t ringbuf_free(const ringbuf_t *rb)
{
    return (uint16_t)(ringbuf_capacity(rb) - ringbuf_count(rb));
}

bool ringbuf_is_empty(const ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

bool ringbuf_is_full(const ringbuf_t *rb)
{
    return ringbuf_count(rb) >= ringbuf_capacity(rb);
}

bool ringbuf_put(ringbuf_t *rb, uint8_t byte)
{
    if (ringbuf_is_full(rb)) {
        return false;
    }
    rb->buffer[rb->head & rb->mask] = byte;
    rb->head++;     /* publish only after the data is stored */
    return true;
}

void ringbuf_put_overwrite(ringbuf_t *rb, uint8_t byte)
{
    if (ringbuf_is_full(rb)) {
        rb->tail++;
    }
    rb->buffer[rb->head & rb->mask] = byte;
    rb->head++;
}

bool ringbuf_get(ringbuf_t *rb, uint8_t *byte)
{
    if (ringbuf_is_empty(rb)) {
        return false;
    }
    *byte = rb->buffer[rb->tail & rb->mask];
    rb->tail++;
    return true;
}

bool ringbuf_peek(const ringbuf_t *rb, uint8_t *byte)
{
    return ringbuf_peek_at(rb, 0U, byte);
}

bool ringbuf_peek_at(const ringbuf_t *rb, uint16_t index, uint8_t *byte)
{
    if (index >= ringbuf_count(rb)) {
        return false;
    }
    *byte = rb->buffer[(uint16_t)(rb->tail + index) & rb->mask];
    return true;
}

uint16_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint16_t len)
{
    uint16_t n = 0U;
    while ((n < len) && ringbuf_put(rb, data[n])) {
        n++;
    }
    return n;
}

uint16_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t n = 0U;
    while ((n < len) && ringbuf_get(rb, &data[n])) {
        n++;
    }
    return n;
}

uint16_t ringbuf_skip(ringbuf_t *rb, uint16_t len)
{
    const uint16_t count = ringbuf_count(rb);
    const uint16_t n     = (len < count) ? len : count;
    rb->tail = (uint16_t)(rb->tail + n);
    return n;
}

/**
 * @file    ringbuf.h
 * @brief   Byte ring buffer (FIFO), power-of-two size, index masking.
 *
 * Single-producer / single-consumer safe without locks: the producer
 * only writes @c head, the consumer only writes @c tail. That makes it
 * suitable between an ISR (e.g. UART RX) and the main loop.
 *
 * The indices run freely and are wrapped with a mask, so
 * count = head - tail works even after overflow of the counters.
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t           *buffer;  /**< storage, size must be a power of 2 */
    uint16_t           mask;    /**< size - 1                          */
    volatile uint16_t  head;    /**< write index (producer)            */
    volatile uint16_t  tail;    /**< read index  (consumer)            */
} ringbuf_t;

/**
 * @brief  Attach a storage array to a ring buffer.
 * @param  size must be a power of two (2..32768).
 * @return false if size is not a power of two.
 */
bool     ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint16_t size);
void     ringbuf_reset(ringbuf_t *rb);

uint16_t ringbuf_count(const ringbuf_t *rb);
uint16_t ringbuf_free(const ringbuf_t *rb);
uint16_t ringbuf_capacity(const ringbuf_t *rb);
bool     ringbuf_is_empty(const ringbuf_t *rb);
bool     ringbuf_is_full(const ringbuf_t *rb);

/** Push one byte; returns false when full (byte dropped). */
bool     ringbuf_put(ringbuf_t *rb, uint8_t byte);
/** Push one byte, overwriting the oldest one when full. */
void     ringbuf_put_overwrite(ringbuf_t *rb, uint8_t byte);
/** Pop one byte; returns false when empty. */
bool     ringbuf_get(ringbuf_t *rb, uint8_t *byte);
/** Read the oldest byte without removing it. */
bool     ringbuf_peek(const ringbuf_t *rb, uint8_t *byte);
/** Read the byte at offset @p index from the oldest (0 = oldest). */
bool     ringbuf_peek_at(const ringbuf_t *rb, uint16_t index, uint8_t *byte);

/** Bulk write / read; return the number of bytes transferred. */
uint16_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint16_t len);
uint16_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint16_t len);
/** Drop @p len oldest bytes (or all if fewer). */
uint16_t ringbuf_skip(ringbuf_t *rb, uint16_t len);

#endif /* RINGBUF_H */

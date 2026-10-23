/**
 * @file    framing.h
 * @brief   Packet framing for byte streams (UART): COBS and SLIP.
 *
 * A UART only carries bytes; to send packets you need a way to find
 * where each packet starts and ends, even after noise or a reset.
 *
 * COBS: removes every 0x00 from the payload, then 0x00 ends a frame.
 *       Overhead: 1 byte per 254 bytes. Encoded size <= len + len/254 + 1.
 * SLIP: RFC 1055, 0xC0 ends a frame, 0xDB escapes. Simple, variable
 *       overhead (worst case 2x).
 */
#ifndef FRAMING_H
#define FRAMING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define COBS_MAX_ENCODED(len)  ((len) + ((len) / 254U) + 1U)

/** Returns the encoded length (without the trailing 0x00 delimiter). */
size_t cobs_encode(const uint8_t *in, size_t len, uint8_t *out, size_t out_size);
/** Returns the decoded length, 0 on malformed input or too-small out. */
size_t cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t out_size);

#define SLIP_END      (0xC0U)
#define SLIP_ESC      (0xDBU)
#define SLIP_ESC_END  (0xDCU)
#define SLIP_ESC_ESC  (0xDDU)

/** Encodes and appends SLIP_END. Returns the total length or 0. */
size_t slip_encode(const uint8_t *in, size_t len, uint8_t *out, size_t out_size);

/** Byte-by-byte SLIP receiver (feed it from the UART RX handler). */
typedef struct {
    uint8_t *buf;
    size_t   size;
    size_t   len;
    bool     escape;
    bool     overflow;
} slip_rx_t;

void slip_rx_init(slip_rx_t *rx, uint8_t *buf, size_t size);
/**
 * @return length of a complete frame when SLIP_END arrives (frame is in
 *         rx->buf until the next call), 0 otherwise.
 */
size_t slip_rx_feed(slip_rx_t *rx, uint8_t byte);

#endif /* FRAMING_H */

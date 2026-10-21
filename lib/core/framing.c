/**
 * @file    framing.c
 * @brief   COBS and SLIP framing (see framing.h).
 */
#include "framing.h"

/* ------------------------------ COBS ------------------------------ */

size_t cobs_encode(const uint8_t *in, size_t len, uint8_t *out, size_t out_size)
{
    size_t  code_pos = 0U;    /* where the current block length goes */
    size_t  o        = 1U;
    uint8_t code     = 1U;

    if (out_size < COBS_MAX_ENCODED(len)) {
        return 0U;
    }
    for (size_t i = 0U; i < len; i++) {
        if (in[i] == 0U) {
            out[code_pos] = code;    /* close the block at the zero */
            code_pos      = o++;
            code          = 1U;
        } else {
            out[o++] = in[i];
            code++;
            if (code == 0xFFU) {     /* max block: 254 data bytes */
                out[code_pos] = code;
                code_pos      = o++;
                code          = 1U;
            }
        }
    }
    out[code_pos] = code;
    return o;
}

size_t cobs_decode(const uint8_t *in, size_t len, uint8_t *out, size_t out_size)
{
    size_t i = 0U;
    size_t o = 0U;

    while (i < len) {
        const uint8_t code = in[i++];
        if (code == 0U) {
            return 0U;                 /* zero inside a frame: invalid */
        }
        for (uint8_t k = 1U; k < code; k++) {
            if ((i >= len) || (o >= out_size) || (in[i] == 0U)) {
                return 0U;
            }
            out[o++] = in[i++];
        }
        /* a block shorter than 0xFF stands for a zero, except at the end */
        if ((code != 0xFFU) && (i < len)) {
            if (o >= out_size) {
                return 0U;
            }
            out[o++] = 0U;
        }
    }
    return o;
}

/* ------------------------------ SLIP ------------------------------ */

size_t slip_encode(const uint8_t *in, size_t len, uint8_t *out, size_t out_size)
{
    size_t o = 0U;

    for (size_t i = 0U; i < len; i++) {
        const uint8_t b = in[i];
        if ((b == SLIP_END) || (b == SLIP_ESC)) {
            if ((o + 2U) > out_size) {
                return 0U;
            }
            out[o++] = SLIP_ESC;
            out[o++] = (b == SLIP_END) ? SLIP_ESC_END : SLIP_ESC_ESC;
        } else {
            if ((o + 1U) > out_size) {
                return 0U;
            }
            out[o++] = b;
        }
    }
    if ((o + 1U) > out_size) {
        return 0U;
    }
    out[o++] = SLIP_END;
    return o;
}

void slip_rx_init(slip_rx_t *rx, uint8_t *buf, size_t size)
{
    rx->buf      = buf;
    rx->size     = size;
    rx->len      = 0U;
    rx->escape   = false;
    rx->overflow = false;
}

size_t slip_rx_feed(slip_rx_t *rx, uint8_t byte)
{
    if (byte == SLIP_END) {
        const size_t n = rx->overflow ? 0U : rx->len;
        rx->len      = 0U;
        rx->escape   = false;
        rx->overflow = false;
        return n;
    }
    if (byte == SLIP_ESC) {
        rx->escape = true;
        return 0U;
    }
    if (rx->escape) {
        byte = (byte == SLIP_ESC_END) ? SLIP_END :
               ((byte == SLIP_ESC_ESC) ? SLIP_ESC : byte);
        rx->escape = false;
    }
    if (rx->len < rx->size) {
        rx->buf[rx->len++] = byte;
    } else {
        rx->overflow = true;       /* drop the whole frame at END */
    }
    return 0U;
}

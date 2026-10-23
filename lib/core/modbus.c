/**
 * @file    modbus.c
 * @brief   Modbus RTU slave (see modbus.h).
 */
#include "modbus.h"
#include "crc.h"

#define FC_READ_COILS        (0x01U)
#define FC_READ_DISCRETE     (0x02U)
#define FC_READ_HOLDING      (0x03U)
#define FC_READ_INPUT        (0x04U)
#define FC_WRITE_COIL        (0x05U)
#define FC_WRITE_REGISTER    (0x06U)
#define FC_WRITE_COILS       (0x0FU)
#define FC_WRITE_REGISTERS   (0x10U)
#define EXCEPTION_FLAG       (0x80U)
#define MAX_READ_REGS        (125U)
#define MAX_READ_BITS        (2000U)
#define MAX_WRITE_REGS       (123U)
#define MAX_WRITE_COILS      (1968U)
#define BROADCAST_ADDR       (0U)

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);      /* big endian */
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static bool bit_get(const uint8_t *bits, uint32_t i)
{
    return (bits[i >> 3] & (1U << (i & 7U))) != 0U;
}

static void bit_put(uint8_t *bits, uint32_t i, bool v)
{
    if (v) {
        bits[i >> 3] |= (uint8_t)(1U << (i & 7U));
    } else {
        bits[i >> 3] &= (uint8_t)~(1U << (i & 7U));
    }
}

void modbus_init(modbus_slave_t *mb, uint8_t address, modbus_send_fn send, void *ctx)
{
    mb->address        = address;
    mb->holding        = NULL;
    mb->holding_count  = 0U;
    mb->input          = NULL;
    mb->input_count    = 0U;
    mb->coils          = NULL;
    mb->coil_count     = 0U;
    mb->discrete       = NULL;
    mb->discrete_count = 0U;
    mb->send           = send;
    mb->on_write       = NULL;
    mb->ctx            = ctx;
    mb->rx_len         = 0U;
    mb->overflow       = false;
    mb->frames_ok      = 0U;
    mb->crc_errors     = 0U;
    mb->exceptions     = 0U;
    modbus_set_baud(mb, 19200U);
}

void modbus_set_baud(modbus_slave_t *mb, uint32_t baud)
{
    /* one character = 11 bits (start, 8 data, parity/stop, stop) */
    mb->silence_us = (baud > 19200U) ? 1750U : ((35U * 11U * 1000000U) / (10U * baud));
}

static void send_frame(modbus_slave_t *mb, uint8_t *frame, size_t len)
{
    const uint16_t crc = crc16_modbus(CRC16_MODBUS_INIT, frame, len);
    frame[len]      = (uint8_t)crc;          /* CRC is sent low byte first */
    frame[len + 1U] = (uint8_t)(crc >> 8);
    if (mb->send != NULL) {
        mb->send(frame, len + 2U, mb->ctx);
    }
}

static void send_exception(modbus_slave_t *mb, uint8_t function, modbus_exception_t ex)
{
    uint8_t f[5];
    f[0] = mb->address;
    f[1] = (uint8_t)(function | EXCEPTION_FLAG);
    f[2] = (uint8_t)ex;
    mb->exceptions++;
    send_frame(mb, f, 3U);
}

/* Handle one request PDU; fills tx and returns its length, or an exception. */
static modbus_exception_t handle(modbus_slave_t *mb, const uint8_t *req, size_t len,
                                 uint8_t *tx, size_t *tx_len)
{
    const uint8_t fc = req[1];
    if (len < 6U) {
        return MODBUS_EX_ILLEGAL_VALUE;
    }
    const uint16_t addr  = get16(&req[2]);
    const uint16_t count = get16(&req[4]);
    tx[0] = mb->address;
    tx[1] = fc;

    switch (fc) {
    case FC_READ_HOLDING:
    case FC_READ_INPUT: {
        const uint16_t *regs = (fc == FC_READ_HOLDING) ? mb->holding : mb->input;
        const uint16_t  n    = (fc == FC_READ_HOLDING) ? mb->holding_count : mb->input_count;
        if ((count == 0U) || (count > MAX_READ_REGS)) {
            return MODBUS_EX_ILLEGAL_VALUE;
        }
        if ((regs == NULL) || (((uint32_t)addr + count) > n)) {
            return MODBUS_EX_ILLEGAL_ADDR;
        }
        tx[2] = (uint8_t)(count * 2U);
        for (uint16_t i = 0U; i < count; i++) {
            put16(&tx[3U + (2U * i)], regs[addr + i]);
        }
        *tx_len = 3U + ((size_t)count * 2U);
        return MODBUS_EX_NONE;
    }
    case FC_READ_COILS:
    case FC_READ_DISCRETE: {
        const uint8_t *bits = (fc == FC_READ_COILS) ? mb->coils : mb->discrete;
        const uint16_t n    = (fc == FC_READ_COILS) ? mb->coil_count : mb->discrete_count;
        if ((count == 0U) || (count > MAX_READ_BITS)) {
            return MODBUS_EX_ILLEGAL_VALUE;
        }
        if ((bits == NULL) || (((uint32_t)addr + count) > n)) {
            return MODBUS_EX_ILLEGAL_ADDR;
        }
        const uint8_t nbytes = (uint8_t)((count + 7U) / 8U);
        tx[2] = nbytes;
        for (uint8_t i = 0U; i < nbytes; i++) {
            tx[3U + i] = 0U;
        }
        for (uint16_t i = 0U; i < count; i++) {
            bit_put(&tx[3], i, bit_get(bits, (uint32_t)addr + i));
        }
        *tx_len = 3U + nbytes;
        return MODBUS_EX_NONE;
    }
    case FC_WRITE_REGISTER:
        if ((mb->holding == NULL) || (addr >= mb->holding_count)) {
            return MODBUS_EX_ILLEGAL_ADDR;
        }
        mb->holding[addr] = count;           /* "count" field is the value */
        for (size_t i = 2U; i < 6U; i++) {
            tx[i] = req[i];                  /* echo the request */
        }
        *tx_len = 6U;
        if (mb->on_write != NULL) {
            mb->on_write(fc, addr, 1U, mb->ctx);
        }
        return MODBUS_EX_NONE;
    case FC_WRITE_COIL:
        if ((count != 0xFF00U) && (count != 0x0000U)) {
            return MODBUS_EX_ILLEGAL_VALUE;
        }
        if ((mb->coils == NULL) || (addr >= mb->coil_count)) {
            return MODBUS_EX_ILLEGAL_ADDR;
        }
        bit_put(mb->coils, addr, count == 0xFF00U);
        for (size_t i = 2U; i < 6U; i++) {
            tx[i] = req[i];
        }
        *tx_len = 6U;
        if (mb->on_write != NULL) {
            mb->on_write(fc, addr, 1U, mb->ctx);
        }
        return MODBUS_EX_NONE;
    case FC_WRITE_REGISTERS:
    case FC_WRITE_COILS: {
        const bool regs = (fc == FC_WRITE_REGISTERS);
        if (len < 7U) {
            return MODBUS_EX_ILLEGAL_VALUE;
        }
        const uint8_t nbytes = req[6];
        if ((count == 0U) || (count > (regs ? MAX_WRITE_REGS : MAX_WRITE_COILS)) ||
            (nbytes != (regs ? (count * 2U) : ((count + 7U) / 8U))) || (len < (7U + (size_t)nbytes))) {
            return MODBUS_EX_ILLEGAL_VALUE;
        }
        if (regs) {
            if ((mb->holding == NULL) || (((uint32_t)addr + count) > mb->holding_count)) {
                return MODBUS_EX_ILLEGAL_ADDR;
            }
            for (uint16_t i = 0U; i < count; i++) {
                mb->holding[addr + i] = get16(&req[7U + (2U * i)]);
            }
        } else {
            if ((mb->coils == NULL) || (((uint32_t)addr + count) > mb->coil_count)) {
                return MODBUS_EX_ILLEGAL_ADDR;
            }
            for (uint16_t i = 0U; i < count; i++) {
                bit_put(mb->coils, (uint32_t)addr + i, bit_get(&req[7], i));
            }
        }
        for (size_t i = 2U; i < 6U; i++) {
            tx[i] = req[i];                   /* reply: address + count */
        }
        *tx_len = 6U;
        if (mb->on_write != NULL) {
            mb->on_write(fc, addr, count, mb->ctx);
        }
        return MODBUS_EX_NONE;
    }
    default:
        return MODBUS_EX_ILLEGAL_FUNC;
    }
}

void modbus_process(modbus_slave_t *mb, const uint8_t *frame, size_t len)
{
    uint8_t tx[MODBUS_MAX_FRAME];
    size_t  tx_len = 0U;

    if (len < 4U) {
        return;                                    /* too short: ignore */
    }
    const uint16_t crc = (uint16_t)(frame[len - 2U] | ((uint16_t)frame[len - 1U] << 8));
    if (crc16_modbus(CRC16_MODBUS_INIT, frame, len - 2U) != crc) {
        mb->crc_errors++;
        return;                                    /* never answer a bad frame */
    }
    if ((frame[0] != mb->address) && (frame[0] != BROADCAST_ADDR)) {
        return;                                    /* for another slave */
    }
    mb->frames_ok++;
    const modbus_exception_t ex = handle(mb, frame, len - 2U, tx, &tx_len);
    if (frame[0] == BROADCAST_ADDR) {
        return;                                    /* broadcasts get no reply */
    }
    if (ex != MODBUS_EX_NONE) {
        send_exception(mb, frame[1], ex);
    } else {
        send_frame(mb, tx, tx_len);
    }
}

void modbus_rx_byte(modbus_slave_t *mb, uint8_t byte, uint32_t now_us)
{
    if ((mb->rx_len > 0U) && ((now_us - mb->last_rx_us) > mb->silence_us)) {
        mb->rx_len   = 0U;                         /* stale partial frame */
        mb->overflow = false;
    }
    mb->last_rx_us = now_us;
    if (mb->rx_len < MODBUS_MAX_FRAME) {
        mb->rx[mb->rx_len++] = byte;
    } else {
        mb->overflow = true;
    }
}

bool modbus_poll(modbus_slave_t *mb, uint32_t now_us)
{
    if ((mb->rx_len == 0U) || ((now_us - mb->last_rx_us) <= mb->silence_us)) {
        return false;
    }
    if (!mb->overflow) {
        modbus_process(mb, mb->rx, mb->rx_len);
    }
    mb->rx_len   = 0U;
    mb->overflow = false;
    return true;
}

size_t modbus_build_request(uint8_t *out, uint8_t slave, uint8_t function,
                            uint16_t addr, uint16_t count_or_value)
{
    out[0] = slave;
    out[1] = function;
    put16(&out[2], addr);
    put16(&out[4], count_or_value);
    const uint16_t crc = crc16_modbus(CRC16_MODBUS_INIT, out, 6U);
    out[6] = (uint8_t)crc;
    out[7] = (uint8_t)(crc >> 8);
    return 8U;
}

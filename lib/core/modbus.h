/**
 * @file    modbus.h
 * @brief   Modbus RTU slave (the industrial fieldbus over RS-485 / UART).
 *
 * Supported functions:
 *   0x01 read coils           0x05 write single coil
 *   0x02 read discrete inputs 0x06 write single register
 *   0x03 read holding regs    0x0F write multiple coils
 *   0x04 read input regs      0x10 write multiple registers
 * Exceptions 01/02/03 are generated automatically.
 *
 * Framing: a frame ends after 3.5 character times of silence; feed
 * bytes with modbus_rx_byte() and call modbus_poll() regularly with
 * the time in microseconds. Data lives in your arrays: the stack only
 * reads/writes them (and calls on_write so the app can react).
 */
#ifndef MODBUS_H
#define MODBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MODBUS_MAX_FRAME   (256U)

typedef enum {
    MODBUS_EX_NONE          = 0x00,
    MODBUS_EX_ILLEGAL_FUNC  = 0x01,
    MODBUS_EX_ILLEGAL_ADDR  = 0x02,
    MODBUS_EX_ILLEGAL_VALUE = 0x03
} modbus_exception_t;

typedef void (*modbus_send_fn)(const uint8_t *frame, size_t len, void *ctx);
/** Called after a master wrote holding registers or coils. */
typedef void (*modbus_write_fn)(uint8_t function, uint16_t addr, uint16_t count, void *ctx);

typedef struct {
    uint8_t         address;           /**< 1..247                       */
    uint16_t       *holding;           /**< R/W registers (0x03/0x06/0x10) */
    uint16_t        holding_count;
    const uint16_t *input;             /**< read-only registers (0x04)    */
    uint16_t        input_count;
    uint8_t        *coils;             /**< bit array, R/W (0x01/0x05/0x0F) */
    uint16_t        coil_count;
    const uint8_t  *discrete;          /**< bit array, read-only (0x02)    */
    uint16_t        discrete_count;
    modbus_send_fn  send;
    modbus_write_fn on_write;          /**< may be NULL */
    void           *ctx;
    uint32_t        silence_us;        /**< 3.5 chars, see modbus_set_baud */
    /* internal */
    uint8_t         rx[MODBUS_MAX_FRAME];
    size_t          rx_len;
    uint32_t        last_rx_us;
    bool            overflow;
    uint32_t        frames_ok;
    uint32_t        crc_errors;
    uint32_t        exceptions;
} modbus_slave_t;

void modbus_init(modbus_slave_t *mb, uint8_t address, modbus_send_fn send, void *ctx);
/** Inter-frame silence from the baud rate (fixed 1750 us above 19200). */
void modbus_set_baud(modbus_slave_t *mb, uint32_t baud);
void modbus_rx_byte(modbus_slave_t *mb, uint8_t byte, uint32_t now_us);
/** Process a frame once the line went silent. Returns true if one was handled. */
bool modbus_poll(modbus_slave_t *mb, uint32_t now_us);
/** Process a complete frame right now (tests, or other framings). */
void modbus_process(modbus_slave_t *mb, const uint8_t *frame, size_t len);

/** Master side helper: build a request with CRC. Returns its length. */
size_t modbus_build_request(uint8_t *out, uint8_t slave, uint8_t function,
                            uint16_t addr, uint16_t count_or_value);

#endif /* MODBUS_H */

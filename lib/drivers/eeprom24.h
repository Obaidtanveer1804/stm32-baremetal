/**
 * @file    eeprom24.h
 * @brief   24Cxx I2C EEPROM (24C02 ... 24C512).
 *
 * Handles what makes these chips tricky:
 *  - page writes wrap inside a page: writes are split on page borders
 *  - after each page write the chip is busy ~5 ms and NACKs its
 *    address: we poll it (ACK polling) instead of a fixed delay
 *  - small chips (<= 24C16) use 1 address byte, bigger ones 2, and
 *    24C04..16 put the high address bits in the I2C address
 */
#ifndef EEPROM24_H
#define EEPROM24_H

#include "hal_i2c.h"

typedef struct {
    hal_i2c_t bus;
    uint8_t   addr;          /**< 7-bit base address, usually 0x50  */
    uint32_t  size;          /**< bytes                              */
    uint16_t  page_size;     /**< 8 (24C02) .. 128 (24C512)          */
    uint8_t   addr_bytes;    /**< 1 or 2                             */
} eeprom24_t;

/** Fill the descriptor from the part number: 1,2,4,8,16,32,64,128,256,512 (kbit). */
bool         eeprom24_init(eeprom24_t *ee, hal_i2c_t bus, uint8_t addr, uint32_t kbit);
hal_status_t eeprom24_read(const eeprom24_t *ee, uint32_t mem, void *data, size_t len);
hal_status_t eeprom24_write(const eeprom24_t *ee, uint32_t mem, const void *data, size_t len);
/** Fill a range with one value (0xFF = "erase"). */
hal_status_t eeprom24_fill(const eeprom24_t *ee, uint32_t mem, uint8_t value, size_t len);
/** Wait until the internal write cycle is over (ACK polling). */
hal_status_t eeprom24_wait_ready(const eeprom24_t *ee, uint32_t timeout_ms);

#endif /* EEPROM24_H */

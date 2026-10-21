/**
 * @file    eeprom24.c
 * @brief   24Cxx EEPROM driver (see eeprom24.h).
 */
#include "eeprom24.h"
#include "hal_time.h"

#define EE_WRITE_TIMEOUT_MS  (20U)
#define EE_MAX_PAGE          (128U)

bool eeprom24_init(eeprom24_t *ee, hal_i2c_t bus, uint8_t addr, uint32_t kbit)
{
    static const struct { uint16_t kbit; uint16_t page; } parts[] = {
        { 1U, 8U }, { 2U, 8U }, { 4U, 16U }, { 8U, 16U }, { 16U, 16U },
        { 32U, 32U }, { 64U, 32U }, { 128U, 64U }, { 256U, 64U }, { 512U, 128U }
    };
    for (uint32_t i = 0U; i < (sizeof(parts) / sizeof(parts[0])); i++) {
        if (parts[i].kbit == kbit) {
            ee->bus        = bus;
            ee->addr       = addr;
            ee->size       = kbit * 128U;            /* kbit * 1024 / 8 */
            ee->page_size  = parts[i].page;
            ee->addr_bytes = (kbit <= 16U) ? 1U : 2U;
            return true;
        }
    }
    return false;
}

/* Device address and header bytes for a memory address. */
static uint8_t make_header(const eeprom24_t *ee, uint32_t mem, uint8_t *hdr, size_t *hdr_len)
{
    if (ee->addr_bytes == 2U) {
        hdr[0]   = (uint8_t)(mem >> 8);
        hdr[1]   = (uint8_t)mem;
        *hdr_len = 2U;
        return ee->addr;
    }
    hdr[0]   = (uint8_t)mem;
    *hdr_len = 1U;
    /* 24C04/08/16: bits 8..10 of the address go in the device address */
    return (uint8_t)(ee->addr | ((mem >> 8) & 0x07U));
}

hal_status_t eeprom24_wait_ready(const eeprom24_t *ee, uint32_t timeout_ms)
{
    const uint32_t t0 = hal_time_ms();
    while (!hal_i2c_probe(ee->bus, ee->addr)) {
        if ((hal_time_ms() - t0) > timeout_ms) {
            return HAL_ETIMEOUT;
        }
    }
    return HAL_OK;
}

hal_status_t eeprom24_read(const eeprom24_t *ee, uint32_t mem, void *data, size_t len)
{
    uint8_t hdr[2];
    size_t  hdr_len;

    if ((mem + len) > ee->size) {
        return HAL_EINVAL;
    }
    if (len == 0U) {
        return HAL_OK;
    }
    /* small parts: a read cannot cross a 256 byte "block" (address byte) */
    uint8_t *p = (uint8_t *)data;
    while (len > 0U) {
        size_t chunk = len;
        if (ee->addr_bytes == 1U) {
            const size_t to_block_end = 256U - (mem & 0xFFU);
            chunk = (chunk < to_block_end) ? chunk : to_block_end;
        }
        const uint8_t dev = make_header(ee, mem, hdr, &hdr_len);
        hal_status_t st = eeprom24_wait_ready(ee, EE_WRITE_TIMEOUT_MS);
        if (st == HAL_OK) {
            st = hal_i2c_write_read(ee->bus, dev, hdr, hdr_len, p, chunk);
        }
        if (st != HAL_OK) {
            return st;
        }
        p   += chunk;
        mem += (uint32_t)chunk;
        len -= chunk;
    }
    return HAL_OK;
}

hal_status_t eeprom24_write(const eeprom24_t *ee, uint32_t mem, const void *data, size_t len)
{
    uint8_t        buf[2U + EE_MAX_PAGE];
    const uint8_t *src = (const uint8_t *)data;

    if ((mem + len) > ee->size) {
        return HAL_EINVAL;
    }
    while (len > 0U) {
        /* never cross a page border: the chip would wrap to page start */
        const size_t room  = ee->page_size - (mem % ee->page_size);
        const size_t chunk = (len < room) ? len : room;
        size_t       hdr_len;
        const uint8_t dev = make_header(ee, mem, buf, &hdr_len);
        for (size_t i = 0U; i < chunk; i++) {
            buf[hdr_len + i] = src[i];
        }
        hal_status_t st = eeprom24_wait_ready(ee, EE_WRITE_TIMEOUT_MS);
        if (st == HAL_OK) {
            st = hal_i2c_write(ee->bus, dev, buf, hdr_len + chunk);
        }
        if (st != HAL_OK) {
            return st;
        }
        src += chunk;
        mem += (uint32_t)chunk;
        len -= chunk;
    }
    return eeprom24_wait_ready(ee, EE_WRITE_TIMEOUT_MS);
}

hal_status_t eeprom24_fill(const eeprom24_t *ee, uint32_t mem, uint8_t value, size_t len)
{
    uint8_t block[16];
    for (size_t i = 0U; i < sizeof(block); i++) {
        block[i] = value;
    }
    while (len > 0U) {
        const size_t n = (len < sizeof(block)) ? len : sizeof(block);
        const hal_status_t st = eeprom24_write(ee, mem, block, n);
        if (st != HAL_OK) {
            return st;
        }
        mem += (uint32_t)n;
        len -= n;
    }
    return HAL_OK;
}

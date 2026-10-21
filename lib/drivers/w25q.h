/**
 * @file    w25q.h
 * @brief   Winbond W25Qxx SPI NOR flash (W25Q16/32/64/128) and
 *          compatible chips (GD25Q, BY25Q...).
 *
 * NOR flash rules: erase sets bytes to 0xFF by 4 KiB sectors (or
 * 64 KiB blocks); programming clears bits by pages of 256 bytes;
 * every program/erase needs WRITE ENABLE first and polls BUSY after.
 */
#ifndef W25Q_H
#define W25Q_H

#include "hal_spi.h"
#include "hal_gpio.h"

#define W25Q_PAGE_SIZE    (256U)
#define W25Q_SECTOR_SIZE  (4096U)
#define W25Q_BLOCK_SIZE   (65536U)

typedef struct {
    hal_spi_t spi;
    hal_pin_t cs;
    uint32_t  size;          /**< bytes, from the JEDEC capacity code */
    uint8_t   jedec[3];      /**< manufacturer, type, capacity        */
} w25q_t;

/** Reads the JEDEC ID; fails if no chip answers (ID 00 00 00 or FF FF FF). */
hal_status_t w25q_init(w25q_t *f, hal_spi_t spi, hal_pin_t cs);
hal_status_t w25q_read(const w25q_t *f, uint32_t addr, void *data, size_t len);
/** Program any range (split on page borders). Area must be erased. */
hal_status_t w25q_write(const w25q_t *f, uint32_t addr, const void *data, size_t len);
hal_status_t w25q_erase_sector(const w25q_t *f, uint32_t addr);
hal_status_t w25q_erase_block(const w25q_t *f, uint32_t addr);
hal_status_t w25q_erase_chip(const w25q_t *f);
bool         w25q_is_busy(const w25q_t *f);
hal_status_t w25q_wait(const w25q_t *f, uint32_t timeout_ms);
void         w25q_unique_id(const w25q_t *f, uint8_t id[8]);
void         w25q_power_down(const w25q_t *f);
void         w25q_wake(const w25q_t *f);
/** Human readable name from the JEDEC ID ("W25Q16 (2 MiB)"). */
const char  *w25q_name(const w25q_t *f);

#endif /* W25Q_H */

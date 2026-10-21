/**
 * @file    hal_spi.h
 * @brief   SPI master, full duplex, 8-bit frames. Chip select is a
 *          normal GPIO handled by the device driver.
 *
 *   SPI1: SCK PA5, MISO PA6, MOSI PA7   (APB2 clock)
 *   SPI2: SCK PB13, MISO PB14, MOSI PB15 (APB1 clock)
 *
 * Generalises spi_f1.c / spi_pc.c of the TP.
 */
#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "hal_types.h"

typedef enum {
    HAL_SPI1 = 0,
    HAL_SPI2,
    HAL_SPI_COUNT
} hal_spi_t;

typedef enum {
    HAL_SPI_MODE0 = 0,   /**< CPOL 0, CPHA 0 (most devices) */
    HAL_SPI_MODE1,       /**< CPOL 0, CPHA 1                */
    HAL_SPI_MODE2,       /**< CPOL 1, CPHA 0                */
    HAL_SPI_MODE3        /**< CPOL 1, CPHA 1                */
} hal_spi_mode_t;

typedef struct {
    uint32_t       max_hz;      /**< highest SCK allowed by the device */
    hal_spi_mode_t mode;
    bool           lsb_first;
} hal_spi_config_t;

hal_status_t hal_spi_init(hal_spi_t spi, const hal_spi_config_t *cfg);
/** Send one byte, return the byte clocked in at the same time. */
uint8_t      hal_spi_transfer(hal_spi_t spi, uint8_t tx);
/** tx or rx may be NULL (send 0xFF / discard). */
void         hal_spi_transfer_buf(hal_spi_t spi, const uint8_t *tx, uint8_t *rx, size_t len);
/** Actual SCK frequency after the power-of-two prescaler. */
uint32_t     hal_spi_get_hz(hal_spi_t spi);

/** Portable: smallest prescaler 2..256 giving SCK <= max_hz. Returns
 *  the exponent (1 for /2 ... 8 for /256), 0 if impossible. */
uint32_t     hal_spi_calc_div_log2(uint32_t pclk_hz, uint32_t max_hz);

#endif /* HAL_SPI_H */

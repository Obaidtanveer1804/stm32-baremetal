/**
 * @file    max7219.h
 * @brief   MAX7219 LED driver on hardware SPI: 8-digit 7-segment
 *          display or 8x8 dot matrix (cascadable).
 */
#ifndef MAX7219_H
#define MAX7219_H

#include "hal_spi.h"
#include "hal_gpio.h"

#define MAX7219_MAX_DEVICES  (4U)

typedef struct {
    hal_spi_t spi;
    hal_pin_t cs;
    uint8_t   devices;
    uint8_t   fb[MAX7219_MAX_DEVICES][8];  /**< matrix frame buffer, row bytes */
} max7219_t;

hal_status_t max7219_init(max7219_t *m, hal_spi_t spi, hal_pin_t cs, uint8_t devices);
/** Write one register on one device of the chain. */
void max7219_write(max7219_t *m, uint8_t device, uint8_t reg, uint8_t value);
void max7219_intensity(max7219_t *m, uint8_t level);        /**< 0..15 */
void max7219_shutdown(max7219_t *m, bool off);
void max7219_test(max7219_t *m, bool on);                   /**< all LEDs on */

/* 7-segment mode (uses the chip's code B decoder) */
void max7219_seg_mode(max7219_t *m, bool decode);
/** Show a signed number right-aligned on 8 digits of device 0. */
void max7219_seg_number(max7219_t *m, int32_t value);
/** Show text with our own 7-seg table (decode off). */
void max7219_seg_text(max7219_t *m, const char *text);

/* matrix mode */
void max7219_clear(max7219_t *m);
void max7219_pixel(max7219_t *m, uint32_t x, uint32_t y, bool on);
void max7219_flush(max7219_t *m);

#endif /* MAX7219_H */

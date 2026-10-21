/**
 * @file    ssd1306.h
 * @brief   SSD1306 128x64 / 128x32 OLED on I2C (address 0x3C), drawn
 *          with core/gfx.h and sent as a whole framebuffer.
 *
 *   ssd1306_t oled;
 *   ssd1306_init(&oled, HAL_I2C1, SSD1306_ADDR, 64);
 *   gfx_puts(&oled.gfx, "Hello");
 *   ssd1306_flush(&oled);
 *
 * RAM cost: 1 KiB for 128x64 (an eighth of the F100 RAM, plan for it).
 */
#ifndef SSD1306_H
#define SSD1306_H

#include "hal_i2c.h"
#include "gfx.h"

#define SSD1306_ADDR     (0x3CU)
#define SSD1306_WIDTH    (128)
#define SSD1306_MAX_ROWS (64)

typedef struct {
    hal_i2c_t bus;
    uint8_t   addr;
    gfx_t     gfx;
    uint8_t   fb[(SSD1306_WIDTH * SSD1306_MAX_ROWS) / 8];
} ssd1306_t;

hal_status_t ssd1306_init(ssd1306_t *d, hal_i2c_t bus, uint8_t addr, int16_t height);
/** Send the framebuffer (only if something was drawn since last time). */
hal_status_t ssd1306_flush(ssd1306_t *d);
hal_status_t ssd1306_contrast(ssd1306_t *d, uint8_t level);
hal_status_t ssd1306_display_on(ssd1306_t *d, bool on);
hal_status_t ssd1306_invert(ssd1306_t *d, bool invert);

#endif /* SSD1306_H */

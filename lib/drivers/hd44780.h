/**
 * @file    hd44780.h
 * @brief   Character LCD (16x2, 20x4) on 6 GPIOs, 4-bit mode, write only
 *          (RW tied to GND).
 *
 *   wiring: RS, E, D4..D7 to any pins; RW -> GND; V0 -> pot (contrast)
 *
 *   hd44780_t lcd;
 *   const hd44780_pins_t pins = { PB(0), PB(1), { PB(4), PB(5), PB(6), PB(7) } };
 *   hd44780_init(&lcd, &pins, 16, 2);
 *   hd44780_printf(&lcd, "T=%d.%d C", t / 10, t % 10);
 */
#ifndef HD44780_H
#define HD44780_H

#include "hal_gpio.h"

typedef struct {
    hal_pin_t rs;
    hal_pin_t en;
    hal_pin_t data[4];    /**< D4, D5, D6, D7 */
} hd44780_pins_t;

typedef struct {
    hd44780_pins_t pins;
    uint8_t        cols;
    uint8_t        rows;
    uint8_t        col;
    uint8_t        row;
    uint8_t        display_ctrl;   /* display / cursor / blink bits */
} hd44780_t;

void hd44780_init(hd44780_t *lcd, const hd44780_pins_t *pins, uint8_t cols, uint8_t rows);
void hd44780_clear(hd44780_t *lcd);
void hd44780_home(hd44780_t *lcd);
void hd44780_goto(hd44780_t *lcd, uint8_t col, uint8_t row);
void hd44780_putc(hd44780_t *lcd, char c);      /**< '\n' goes to the next line */
void hd44780_puts(hd44780_t *lcd, const char *s);
int  hd44780_printf(hd44780_t *lcd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void hd44780_cursor(hd44780_t *lcd, bool show, bool blink);
void hd44780_display(hd44780_t *lcd, bool on);
/** Define custom character 0..7 from 8 rows of 5 bits. */
void hd44780_create_char(hd44780_t *lcd, uint8_t index, const uint8_t rows[8]);
/** Horizontal bar graph on one row, 0..1000 per-mille (uses custom chars 0..4). */
void hd44780_bar(hd44780_t *lcd, uint8_t row, uint32_t permille);

#endif /* HD44780_H */

/**
 * @file    hd44780.c
 * @brief   HD44780 4-bit driver (see hd44780.h).
 */
#include "hd44780.h"
#include "hal_time.h"
#include "fmt.h"

#include <stdarg.h>

#define CMD_CLEAR          (0x01U)
#define CMD_HOME           (0x02U)
#define CMD_ENTRY_MODE     (0x04U)
#define   ENTRY_INC        (0x02U)
#define CMD_DISPLAY        (0x08U)
#define   DISPLAY_ON       (0x04U)
#define   CURSOR_ON        (0x02U)
#define   BLINK_ON         (0x01U)
#define CMD_FUNCTION       (0x20U)
#define   FUNC_2LINES      (0x08U)
#define CMD_SET_CGRAM      (0x40U)
#define CMD_SET_DDRAM      (0x80U)

#define LCD_PRINTF_MAX     (41U)

/* DDRAM start address of each row (rows 2/3 continue rows 0/1) */
static const uint8_t row_offset[4] = { 0x00U, 0x40U, 0x14U, 0x54U };

static void pulse_enable(const hd44780_t *lcd)
{
    hal_gpio_set(lcd->pins.en);
    hal_delay_us(1U);                 /* PWEH >= 450 ns */
    hal_gpio_clear(lcd->pins.en);     /* data latched on the falling edge */
    hal_delay_us(50U);                /* most commands take 37 us */
}

static void write_nibble(const hd44780_t *lcd, uint8_t nibble)
{
    for (uint32_t i = 0U; i < 4U; i++) {
        hal_gpio_write(lcd->pins.data[i], ((nibble >> i) & 1U) != 0U);
    }
    pulse_enable(lcd);
}

static void write_byte(const hd44780_t *lcd, uint8_t value, bool is_data)
{
    hal_gpio_write(lcd->pins.rs, is_data);
    write_nibble(lcd, (uint8_t)(value >> 4));
    write_nibble(lcd, (uint8_t)(value & 0x0FU));
}

static void command(const hd44780_t *lcd, uint8_t cmd)
{
    write_byte(lcd, cmd, false);
    if ((cmd == CMD_CLEAR) || (cmd == CMD_HOME)) {
        hal_delay_ms(2U);             /* these two take 1.52 ms */
    }
}

void hd44780_init(hd44780_t *lcd, const hd44780_pins_t *pins, uint8_t cols, uint8_t rows)
{
    lcd->pins = *pins;
    lcd->cols = cols;
    lcd->rows = (rows > 4U) ? 4U : rows;
    lcd->col  = 0U;
    lcd->row  = 0U;

    hal_gpio_mode(pins->rs, HAL_GPIO_OUTPUT);
    hal_gpio_mode(pins->en, HAL_GPIO_OUTPUT);
    for (uint32_t i = 0U; i < 4U; i++) {
        hal_gpio_mode(pins->data[i], HAL_GPIO_OUTPUT);
    }
    hal_gpio_clear(pins->rs);
    hal_gpio_clear(pins->en);

    /* datasheet "initialisation by instruction" (fig. 24): the chip may
     * be in 8-bit mode or half way through a 4-bit transfer, so send
     * 0x3 three times, then switch to 4-bit with 0x2 */
    hal_delay_ms(50U);
    write_nibble(lcd, 0x3U);
    hal_delay_ms(5U);
    write_nibble(lcd, 0x3U);
    hal_delay_us(150U);
    write_nibble(lcd, 0x3U);
    write_nibble(lcd, 0x2U);

    command(lcd, (uint8_t)(CMD_FUNCTION | ((lcd->rows > 1U) ? FUNC_2LINES : 0U)));
    lcd->display_ctrl = DISPLAY_ON;
    command(lcd, (uint8_t)(CMD_DISPLAY | lcd->display_ctrl));
    command(lcd, CMD_CLEAR);
    command(lcd, (uint8_t)(CMD_ENTRY_MODE | ENTRY_INC));
}

void hd44780_clear(hd44780_t *lcd)
{
    command(lcd, CMD_CLEAR);
    lcd->col = 0U;
    lcd->row = 0U;
}

void hd44780_home(hd44780_t *lcd)
{
    command(lcd, CMD_HOME);
    lcd->col = 0U;
    lcd->row = 0U;
}

void hd44780_goto(hd44780_t *lcd, uint8_t col, uint8_t row)
{
    if (row >= lcd->rows) {
        row = (uint8_t)(lcd->rows - 1U);
    }
    if (col >= lcd->cols) {
        col = (uint8_t)(lcd->cols - 1U);
    }
    lcd->col = col;
    lcd->row = row;
    command(lcd, (uint8_t)(CMD_SET_DDRAM | (row_offset[row] + col)));
}

void hd44780_putc(hd44780_t *lcd, char c)
{
    if (c == '\n') {
        hd44780_goto(lcd, 0U, (uint8_t)((lcd->row + 1U) % lcd->rows));
        return;
    }
    if (c == '\r') {
        hd44780_goto(lcd, 0U, lcd->row);
        return;
    }
    if (lcd->col >= lcd->cols) {
        /* the DDRAM is not linear between rows: wrap by hand */
        hd44780_goto(lcd, 0U, (uint8_t)((lcd->row + 1U) % lcd->rows));
    }
    write_byte(lcd, (uint8_t)c, true);
    lcd->col++;
}

void hd44780_puts(hd44780_t *lcd, const char *s)
{
    while (*s != '\0') {
        hd44780_putc(lcd, *s++);
    }
}

int hd44780_printf(hd44780_t *lcd, const char *fmt, ...)
{
    char    buf[LCD_PRINTF_MAX];
    va_list args;
    va_start(args, fmt);
    const int n = fmt_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    hd44780_puts(lcd, buf);
    return n;
}

void hd44780_cursor(hd44780_t *lcd, bool show, bool blink)
{
    lcd->display_ctrl = (uint8_t)((lcd->display_ctrl & DISPLAY_ON) | (show ? CURSOR_ON : 0U) |
                                  (blink ? BLINK_ON : 0U));
    command(lcd, (uint8_t)(CMD_DISPLAY | lcd->display_ctrl));
}

void hd44780_display(hd44780_t *lcd, bool on)
{
    lcd->display_ctrl = (uint8_t)(on ? (lcd->display_ctrl | DISPLAY_ON)
                                     : (lcd->display_ctrl & (uint8_t)~DISPLAY_ON));
    command(lcd, (uint8_t)(CMD_DISPLAY | lcd->display_ctrl));
}

void hd44780_create_char(hd44780_t *lcd, uint8_t index, const uint8_t rows[8])
{
    command(lcd, (uint8_t)(CMD_SET_CGRAM | ((index & 0x07U) << 3)));
    for (uint32_t i = 0U; i < 8U; i++) {
        write_byte(lcd, (uint8_t)(rows[i] & 0x1FU), true);
    }
    hd44780_goto(lcd, lcd->col, lcd->row);   /* back to DDRAM */
}

void hd44780_bar(hd44780_t *lcd, uint8_t row, uint32_t permille)
{
    static bool chars_loaded;
    if (!chars_loaded) {
        /* char k = k+1 columns filled (k = 0..4) */
        for (uint8_t k = 0U; k < 5U; k++) {
            uint8_t glyph[8];
            const uint8_t line = (uint8_t)(0x1FU & ~(0x1FU >> (k + 1U)));
            for (uint32_t r = 0U; r < 8U; r++) {
                glyph[r] = line;
            }
            hd44780_create_char(lcd, k, glyph);
        }
        chars_loaded = true;
    }
    if (permille > 1000U) {
        permille = 1000U;
    }
    const uint32_t steps = ((uint32_t)lcd->cols * 5U * permille) / 1000U;  /* 5 px per cell */
    hd44780_goto(lcd, 0U, row);
    for (uint32_t c = 0U; c < lcd->cols; c++) {
        const uint32_t filled = (steps > (c * 5U)) ? (steps - (c * 5U)) : 0U;
        if (filled >= 5U) {
            write_byte(lcd, 4U, true);
        } else if (filled == 0U) {
            write_byte(lcd, (uint8_t)' ', true);
        } else {
            write_byte(lcd, (uint8_t)(filled - 1U), true);
        }
    }
    lcd->col = lcd->cols;
}

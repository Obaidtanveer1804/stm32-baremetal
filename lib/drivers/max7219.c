/**
 * @file    max7219.c
 * @brief   MAX7219 driver (see max7219.h).
 *
 * Each device takes a 16-bit frame (register, data). With N chips in
 * a chain we shift N frames while CS is low; the frame shifted first
 * ends in the LAST chip. A no-op (register 0) is sent to the others.
 */
#include "max7219.h"
#include "codes.h"
#include "strutil.h"

#define REG_NOOP         (0x00U)
#define REG_DIGIT0       (0x01U)
#define REG_DECODE       (0x09U)
#define REG_INTENSITY    (0x0AU)
#define REG_SCAN_LIMIT   (0x0BU)
#define REG_SHUTDOWN     (0x0CU)
#define REG_TEST         (0x0FU)
#define CODEB_BLANK      (0x0FU)
#define CODEB_MINUS      (0x0AU)
#define DP_BIT           (0x80U)

void max7219_write(max7219_t *m, uint8_t device, uint8_t reg, uint8_t value)
{
    hal_gpio_clear(m->cs);
    for (uint32_t d = m->devices; d > 0U; d--) {
        const bool target = ((d - 1U) == device);
        (void)hal_spi_transfer(m->spi, target ? reg : REG_NOOP);
        (void)hal_spi_transfer(m->spi, target ? value : 0U);
    }
    hal_gpio_set(m->cs);              /* data latched on CS rising edge */
}

static void write_all(max7219_t *m, uint8_t reg, uint8_t value)
{
    hal_gpio_clear(m->cs);
    for (uint32_t d = 0U; d < m->devices; d++) {
        (void)hal_spi_transfer(m->spi, reg);
        (void)hal_spi_transfer(m->spi, value);
    }
    hal_gpio_set(m->cs);
}

hal_status_t max7219_init(max7219_t *m, hal_spi_t spi, hal_pin_t cs, uint8_t devices)
{
    if ((devices == 0U) || (devices > MAX7219_MAX_DEVICES)) {
        return HAL_EINVAL;
    }
    m->spi     = spi;
    m->cs      = cs;
    m->devices = devices;
    hal_gpio_set(cs);
    hal_gpio_mode(cs, HAL_GPIO_OUTPUT_FAST);
    write_all(m, REG_TEST, 0U);
    write_all(m, REG_SCAN_LIMIT, 7U);      /* all 8 digits / rows */
    write_all(m, REG_DECODE, 0U);
    write_all(m, REG_INTENSITY, 4U);
    max7219_clear(m);
    max7219_flush(m);
    write_all(m, REG_SHUTDOWN, 1U);        /* normal operation */
    return HAL_OK;
}

void max7219_intensity(max7219_t *m, uint8_t level)
{
    write_all(m, REG_INTENSITY, (uint8_t)(level & 0x0FU));
}

void max7219_shutdown(max7219_t *m, bool off)
{
    write_all(m, REG_SHUTDOWN, (uint8_t)(off ? 0U : 1U));
}

void max7219_test(max7219_t *m, bool on)
{
    write_all(m, REG_TEST, (uint8_t)(on ? 1U : 0U));
}

void max7219_seg_mode(max7219_t *m, bool decode)
{
    max7219_write(m, 0U, REG_DECODE, decode ? 0xFFU : 0x00U);
}

void max7219_seg_number(max7219_t *m, int32_t value)
{
    const bool negative = value < 0;
    uint32_t   mag      = negative ? (0U - (uint32_t)value) : (uint32_t)value;

    max7219_seg_mode(m, true);
    for (uint8_t digit = 0U; digit < 8U; digit++) {
        uint8_t code;
        if ((mag != 0U) || (digit == 0U)) {
            code = (uint8_t)(mag % 10U);
            mag /= 10U;
        } else if (negative) {
            code = CODEB_MINUS;
            mag  = 0U;
            /* only one minus sign: following digits blank */
            max7219_write(m, 0U, (uint8_t)(REG_DIGIT0 + digit), code);
            for (uint8_t rest = (uint8_t)(digit + 1U); rest < 8U; rest++) {
                max7219_write(m, 0U, (uint8_t)(REG_DIGIT0 + rest), CODEB_BLANK);
            }
            return;
        } else {
            code = CODEB_BLANK;
        }
        max7219_write(m, 0U, (uint8_t)(REG_DIGIT0 + digit), code);
    }
}

/* our table is gfedcba, the MAX7219 wants dp-a-b-c-d-e-f-g */
static uint8_t seg_to_max(uint8_t s)
{
    uint8_t out = (s & SEG7_DP) ? DP_BIT : 0U;
    if (s & SEG7_A) { out |= 0x40U; }
    if (s & SEG7_B) { out |= 0x20U; }
    if (s & SEG7_C) { out |= 0x10U; }
    if (s & SEG7_D) { out |= 0x08U; }
    if (s & SEG7_E) { out |= 0x04U; }
    if (s & SEG7_F) { out |= 0x02U; }
    if (s & SEG7_G) { out |= 0x01U; }
    return out;
}

void max7219_seg_text(max7219_t *m, const char *text)
{
    uint8_t digits[8] = { 0U };
    uint32_t n = 0U;

    max7219_seg_mode(m, false);
    /* left to right on the display = digit 7 down to digit 0 */
    for (const char *p = text; (*p != '\0') && (n < 8U); p++) {
        if ((*p == '.') && (n > 0U)) {
            digits[n - 1U] |= DP_BIT;        /* dot merges with previous digit */
            continue;
        }
        digits[n++] = seg_to_max(codes_seg7_char(*p));
    }
    for (uint32_t i = 0U; i < 8U; i++) {
        max7219_write(m, 0U, (uint8_t)(REG_DIGIT0 + 7U - i), digits[i]);
    }
}

void max7219_clear(max7219_t *m)
{
    for (uint32_t d = 0U; d < m->devices; d++) {
        for (uint32_t r = 0U; r < 8U; r++) {
            m->fb[d][r] = 0U;
        }
    }
}

void max7219_pixel(max7219_t *m, uint32_t x, uint32_t y, bool on)
{
    const uint32_t dev = x / 8U;
    if ((dev >= m->devices) || (y >= 8U)) {
        return;
    }
    const uint8_t mask = (uint8_t)(0x80U >> (x % 8U));
    if (on) {
        m->fb[dev][y] |= mask;
    } else {
        m->fb[dev][y] &= (uint8_t)~mask;
    }
}

void max7219_flush(max7219_t *m)
{
    for (uint32_t row = 0U; row < 8U; row++) {
        hal_gpio_clear(m->cs);
        for (uint32_t d = m->devices; d > 0U; d--) {
            (void)hal_spi_transfer(m->spi, (uint8_t)(REG_DIGIT0 + row));
            (void)hal_spi_transfer(m->spi, m->fb[d - 1U][row]);
        }
        hal_gpio_set(m->cs);
    }
}

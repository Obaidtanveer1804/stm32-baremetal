/**
 * @file    ssd1306.c
 * @brief   SSD1306 OLED driver (see ssd1306.h).
 *
 * I2C framing: first byte is a control byte, 0x00 = commands follow,
 * 0x40 = display data follows. The buffer is sent in horizontal
 * addressing mode, in chunks of 16 bytes (small stack, works with
 * every I2C implementation).
 */
#include "ssd1306.h"

#define CTRL_CMD          (0x00U)
#define CTRL_DATA         (0x40U)
#define CHUNK             (16U)
#define SSD1306_CMD_MAX   (32U)    /* longest command list sent at once */

#define CMD_DISPLAY_OFF   (0xAEU)
#define CMD_DISPLAY_ON    (0xAFU)
#define CMD_CLOCK_DIV     (0xD5U)
#define CMD_MULTIPLEX     (0xA8U)
#define CMD_OFFSET        (0xD3U)
#define CMD_START_LINE    (0x40U)
#define CMD_CHARGE_PUMP   (0x8DU)
#define CMD_MEM_MODE      (0x20U)
#define CMD_SEG_REMAP     (0xA1U)
#define CMD_COM_SCAN_DEC  (0xC8U)
#define CMD_COM_PINS      (0xDAU)
#define CMD_CONTRAST      (0x81U)
#define CMD_PRECHARGE     (0xD9U)
#define CMD_VCOM_DETECT   (0xDBU)
#define CMD_RESUME_RAM    (0xA4U)
#define CMD_NORMAL        (0xA6U)
#define CMD_INVERT        (0xA7U)
#define CMD_COL_ADDR      (0x21U)
#define CMD_PAGE_ADDR     (0x22U)

static hal_status_t commands(const ssd1306_t *d, const uint8_t *cmds, size_t n)
{
    uint8_t buf[1U + SSD1306_CMD_MAX];
    if (n > SSD1306_CMD_MAX) {
        return HAL_EINVAL;
    }
    buf[0] = CTRL_CMD;
    for (size_t i = 0U; i < n; i++) {
        buf[1U + i] = cmds[i];
    }
    return hal_i2c_write(d->bus, d->addr, buf, n + 1U);
}

static hal_status_t command1(const ssd1306_t *d, uint8_t c)
{
    return commands(d, &c, 1U);
}

hal_status_t ssd1306_init(ssd1306_t *d, hal_i2c_t bus, uint8_t addr, int16_t height)
{
    d->bus  = bus;
    d->addr = addr;
    if ((height != 32) && (height != 64)) {
        return HAL_EINVAL;
    }
    gfx_init(&d->gfx, d->fb, SSD1306_WIDTH, height);

    const uint8_t init[] = {
        CMD_DISPLAY_OFF,
        CMD_CLOCK_DIV, 0x80U,
        CMD_MULTIPLEX, (uint8_t)(height - 1),
        CMD_OFFSET, 0x00U,
        CMD_START_LINE,
        CMD_CHARGE_PUMP, 0x14U,           /* internal charge pump on   */
        CMD_MEM_MODE, 0x00U,              /* horizontal addressing     */
        CMD_SEG_REMAP,                    /* column 127 -> SEG0        */
        CMD_COM_SCAN_DEC,                 /* flip vertically           */
        CMD_COM_PINS, (uint8_t)((height == 64) ? 0x12U : 0x02U),
        CMD_CONTRAST, 0x8FU,
        CMD_PRECHARGE, 0xF1U,
        CMD_VCOM_DETECT, 0x40U,
        CMD_RESUME_RAM,
        CMD_NORMAL,
        CMD_DISPLAY_ON,
    };
    const hal_status_t st = commands(d, init, sizeof(init));
    if (st != HAL_OK) {
        return st;
    }
    d->gfx.dirty = true;
    return ssd1306_flush(d);
}

hal_status_t ssd1306_flush(ssd1306_t *d)
{
    uint8_t buf[1U + CHUNK];

    if (!d->gfx.dirty) {
        return HAL_OK;
    }
    const uint8_t window[] = {
        CMD_COL_ADDR, 0U, (uint8_t)(SSD1306_WIDTH - 1),
        CMD_PAGE_ADDR, 0U, (uint8_t)((d->gfx.height / 8) - 1),
    };
    hal_status_t st = commands(d, window, sizeof(window));
    const uint32_t total = ((uint32_t)d->gfx.width * (uint32_t)d->gfx.height) / 8U;
    buf[0] = CTRL_DATA;
    for (uint32_t off = 0U; (st == HAL_OK) && (off < total); off += CHUNK) {
        for (uint32_t i = 0U; i < CHUNK; i++) {
            buf[1U + i] = d->fb[off + i];
        }
        st = hal_i2c_write(d->bus, d->addr, buf, sizeof(buf));
    }
    if (st == HAL_OK) {
        d->gfx.dirty = false;
    }
    return st;
}

hal_status_t ssd1306_contrast(ssd1306_t *d, uint8_t level)
{
    const uint8_t c[2] = { CMD_CONTRAST, level };
    return commands(d, c, sizeof(c));
}

hal_status_t ssd1306_display_on(ssd1306_t *d, bool on)
{
    return command1(d, on ? CMD_DISPLAY_ON : CMD_DISPLAY_OFF);
}

hal_status_t ssd1306_invert(ssd1306_t *d, bool invert)
{
    return command1(d, invert ? CMD_INVERT : CMD_NORMAL);
}

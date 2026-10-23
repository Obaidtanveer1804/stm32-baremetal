/**
 * @file    sim_devices_pc.c
 * @brief   Behavioural models of common breadboard chips, attached to
 *          the simulated buses. They follow the datasheets closely
 *          enough to exercise real drivers: address pointers, page
 *          wrap, write-cycle NACK, busy flags, JEDEC IDs...
 *
 *   24C02  (I2C 0x50): 256 B EEPROM, 8-byte pages, 5 ms write cycle
 *   LM75   (I2C 0x48): temperature sensor, 9-bit, pointer register
 *   DS1307 (I2C 0x68): RTC with BCD registers + 56 B RAM
 *   W25Q16 (SPI1, CS PA4): 2 MiB NOR flash, 256 B pages, 4 KiB sectors
 *   SSD1306 (I2C 0x3C): 128x64 OLED, GDDRAM viewable with sim_oled_show()
 */
#include "sim_internal.h"
#include "codes.h"
#include "datetime.h"
#include "gfx.h"
#include "io.h"

#include <time.h>

/* ============================== 24C02 ============================= */

#define EE_SIZE         (256U)
#define EE_PAGE         (8U)
#define EE_WRITE_MS     (5U)

typedef struct {
    uint8_t  mem[EE_SIZE];
    uint8_t  ptr;
    bool     addr_phase;    /* next written byte is the word address */
    bool     wrote_data;
    uint32_t busy_until;
} ee_state_t;

static ee_state_t s_ee;

static bool ee_start(sim_i2c_dev_t *dev, bool read)
{
    ee_state_t *e = (ee_state_t *)dev->ctx;
    if ((int32_t)(hal_time_ms() - e->busy_until) < 0) {
        return false;                   /* internal write cycle: NACK */
    }
    e->addr_phase = !read;
    e->wrote_data = false;
    return true;
}

static bool ee_write(sim_i2c_dev_t *dev, uint8_t byte)
{
    ee_state_t *e = (ee_state_t *)dev->ctx;
    if (e->addr_phase) {
        e->ptr        = byte;
        e->addr_phase = false;
    } else {
        e->mem[e->ptr] = byte;
        /* page write: only the low 3 bits advance (roll over in page) */
        e->ptr = (uint8_t)((e->ptr & (uint8_t)~(EE_PAGE - 1U)) | ((e->ptr + 1U) & (EE_PAGE - 1U)));
        e->wrote_data = true;
    }
    return true;
}

static uint8_t ee_read(sim_i2c_dev_t *dev)
{
    ee_state_t *e = (ee_state_t *)dev->ctx;
    return e->mem[e->ptr++];           /* sequential read wraps at 256 */
}

static void ee_stop(sim_i2c_dev_t *dev)
{
    ee_state_t *e = (ee_state_t *)dev->ctx;
    if (e->wrote_data) {
        e->busy_until = hal_time_ms() + EE_WRITE_MS;
        e->wrote_data = false;
    }
}

static sim_i2c_dev_t s_dev_eeprom = { "24C02 EEPROM", 0x50U, ee_start, ee_write, ee_read, ee_stop, &s_ee, NULL };

/* =============================== LM75 ============================= */

typedef struct {
    uint8_t  pointer;
    uint8_t  config;
    int16_t  thyst;
    int16_t  tos;
    uint8_t  index;        /* byte index inside a 16-bit register */
    bool     ptr_phase;
    uint8_t  wbuf[2];
} lm75_state_t;

static lm75_state_t s_lm75 = { 0U, 0U, (int16_t)0x4B00, (int16_t)0x5000, 0U, false, { 0U, 0U } };

static int16_t lm75_temp_reg(void)
{
    /* 9-bit two's complement, 0.5 C per LSB, left aligned in 16 bits */
    const int32_t half_deg = (sim_get_temperature_c10() * 2) / 10;
    return (int16_t)(half_deg * 128);
}

static bool lm75_start(sim_i2c_dev_t *dev, bool read)
{
    lm75_state_t *s = (lm75_state_t *)dev->ctx;
    s->index     = 0U;
    s->ptr_phase = !read;
    return true;
}

static bool lm75_write(sim_i2c_dev_t *dev, uint8_t byte)
{
    lm75_state_t *s = (lm75_state_t *)dev->ctx;
    if (s->ptr_phase) {
        s->pointer   = byte & 0x03U;
        s->ptr_phase = false;
        return true;
    }
    if (s->pointer == 1U) {
        s->config = byte;
    } else if (s->index < 2U) {
        s->wbuf[s->index++] = byte;
        if (s->index == 2U) {
            const int16_t v = (int16_t)(((uint16_t)s->wbuf[0] << 8) | s->wbuf[1]);
            if (s->pointer == 2U) { s->thyst = v; }
            if (s->pointer == 3U) { s->tos   = v; }
        }
    } else {
        /* extra bytes ignored */
    }
    return true;
}

static uint8_t lm75_read(sim_i2c_dev_t *dev)
{
    lm75_state_t *s = (lm75_state_t *)dev->ctx;
    int16_t v;
    switch (s->pointer) {
    case 1U:  return s->config;
    case 2U:  v = s->thyst;          break;
    case 3U:  v = s->tos;            break;
    default:  v = lm75_temp_reg();   break;
    }
    const uint8_t b = (s->index == 0U) ? (uint8_t)((uint16_t)v >> 8) : (uint8_t)v;
    s->index ^= 1U;
    return b;
}

static sim_i2c_dev_t s_dev_lm75 = { "LM75 temperature", 0x48U, lm75_start, lm75_write, lm75_read, NULL, &s_lm75, NULL };

/* ============================== DS1307 ============================ */

#define DS_REGS  (64U)

typedef struct {
    uint8_t  regs[DS_REGS];    /* 0..6 time, 7 control, 8..63 RAM */
    uint8_t  ptr;
    bool     ptr_phase;
    int32_t  offset_s;         /* chip time - host time           */
    bool     time_written;
} ds_state_t;

static ds_state_t s_ds;

static void ds_snapshot(ds_state_t *s)
{
    datetime_t dt;
    dt_from_unix((uint32_t)((int64_t)time(NULL) + s->offset_s), &dt);
    s->regs[0] = (uint8_t)codes_bin_to_bcd(dt.second);   /* CH bit 7 = 0: running */
    s->regs[1] = (uint8_t)codes_bin_to_bcd(dt.minute);
    s->regs[2] = (uint8_t)codes_bin_to_bcd(dt.hour);     /* 24 h mode */
    s->regs[3] = (uint8_t)(dt.weekday + 1U);             /* 1..7 */
    s->regs[4] = (uint8_t)codes_bin_to_bcd(dt.day);
    s->regs[5] = (uint8_t)codes_bin_to_bcd(dt.month);
    s->regs[6] = (uint8_t)codes_bin_to_bcd(dt.year % 100U);
}

static uint8_t bcd_reg(uint8_t v)
{
    uint32_t b = 0U;
    (void)codes_bcd_to_bin(v, &b);
    return (uint8_t)b;
}

static void ds_commit_time(ds_state_t *s)
{
    datetime_t dt;
    uint32_t   t;
    dt.second = bcd_reg(s->regs[0] & 0x7FU);
    dt.minute = bcd_reg(s->regs[1]);
    dt.hour   = bcd_reg(s->regs[2] & 0x3FU);
    dt.day    = bcd_reg(s->regs[4]);
    dt.month  = bcd_reg(s->regs[5]);
    dt.year   = (uint16_t)(2000U + bcd_reg(s->regs[6]));
    if (dt_to_unix(&dt, &t)) {
        s->offset_s = (int32_t)((int64_t)t - (int64_t)time(NULL));
    }
}

static bool ds_start(sim_i2c_dev_t *dev, bool read)
{
    ds_state_t *s = (ds_state_t *)dev->ctx;
    s->ptr_phase = !read;
    if (read) {
        ds_snapshot(s);
    }
    return true;
}

static bool ds_write(sim_i2c_dev_t *dev, uint8_t byte)
{
    ds_state_t *s = (ds_state_t *)dev->ctx;
    if (s->ptr_phase) {
        s->ptr       = byte & (DS_REGS - 1U);
        s->ptr_phase = false;
        ds_snapshot(s);
        return true;
    }
    s->regs[s->ptr] = byte;
    if (s->ptr <= 6U) {
        s->time_written = true;
    }
    s->ptr = (uint8_t)((s->ptr + 1U) & (DS_REGS - 1U));
    return true;
}

static uint8_t ds_read(sim_i2c_dev_t *dev)
{
    ds_state_t *s = (ds_state_t *)dev->ctx;
    const uint8_t b = s->regs[s->ptr];
    s->ptr = (uint8_t)((s->ptr + 1U) & (DS_REGS - 1U));
    return b;
}

static void ds_stop(sim_i2c_dev_t *dev)
{
    ds_state_t *s = (ds_state_t *)dev->ctx;
    if (s->time_written) {
        ds_commit_time(s);
        s->time_written = false;
    }
}

static sim_i2c_dev_t s_dev_ds1307 = { "DS1307 RTC", 0x68U, ds_start, ds_write, ds_read, ds_stop, &s_ds, NULL };

/* ============================== W25Q16 ============================ */

#define W25_SIZE        (2U * 1024U * 1024U)
#define W25_PAGE        (256U)
#define W25_SECTOR      (4096U)
#define W25_BLOCK       (65536U)
#define W25_SR_BUSY     (0x01U)
#define W25_SR_WEL      (0x02U)

typedef enum {
    W25_OP_NONE = 0,
    W25_OP_PROGRAM,
    W25_OP_ERASE_4K,
    W25_OP_ERASE_64K,
    W25_OP_ERASE_CHIP
} w25_op_t;

typedef struct {
    uint8_t  mem[W25_SIZE];
    bool     selected;
    uint8_t  cmd;
    uint32_t index;          /* bytes received since CS low      */
    uint32_t addr;
    uint8_t  status;
    w25_op_t pending;
    uint32_t busy_until;
    bool     powered_down;
    bool     initialised;
} w25_state_t;

static w25_state_t s_w25;

static bool w25_busy(const w25_state_t *s)
{
    return (int32_t)(hal_time_ms() - s->busy_until) < 0;
}

static void w25_init_mem(w25_state_t *s)
{
    if (!s->initialised) {
        for (uint32_t i = 0U; i < W25_SIZE; i++) {
            s->mem[i] = 0xFFU;
        }
        s->initialised = true;
    }
}

static void w25_finish(w25_state_t *s)
{
    uint32_t start = 0U;
    uint32_t len   = 0U;
    uint32_t ms    = 0U;

    switch (s->pending) {
    case W25_OP_PROGRAM:    ms = 1U; break;
    case W25_OP_ERASE_4K:   start = s->addr & ~(W25_SECTOR - 1U); len = W25_SECTOR; ms = 30U;  break;
    case W25_OP_ERASE_64K:  start = s->addr & ~(W25_BLOCK - 1U);  len = W25_BLOCK;  ms = 150U; break;
    case W25_OP_ERASE_CHIP: start = 0U; len = W25_SIZE; ms = 400U; break;
    case W25_OP_NONE:
    default:                return;
    }
    for (uint32_t i = 0U; i < len; i++) {
        s->mem[start + i] = 0xFFU;
    }
    s->busy_until = hal_time_ms() + ms;
    s->status    &= (uint8_t)~W25_SR_WEL;      /* WEL auto-clears */
    s->pending    = W25_OP_NONE;
}

static void w25_select(sim_spi_dev_t *dev, bool selected)
{
    w25_state_t *s = (w25_state_t *)dev->ctx;
    w25_init_mem(s);
    if (selected) {
        s->selected = true;
        s->index    = 0U;
        s->cmd      = 0U;
    } else {
        s->selected = false;
        w25_finish(s);                 /* program/erase start at CS high */
    }
}

static uint8_t w25_transfer(sim_spi_dev_t *dev, uint8_t mosi)
{
    w25_state_t   *s   = (w25_state_t *)dev->ctx;
    const uint32_t idx = s->index++;
    uint8_t        miso = 0xFFU;

    if (idx == 0U) {
        s->cmd = mosi;
        if (s->powered_down && (mosi != 0xABU)) {
            return 0xFFU;
        }
        if (w25_busy(s) && (mosi != 0x05U)) {
            return 0xFFU;              /* only status reads while busy */
        }
        switch (mosi) {
        case 0x06U: s->status |= W25_SR_WEL;              break;  /* write enable  */
        case 0x04U: s->status &= (uint8_t)~W25_SR_WEL;    break;  /* write disable */
        case 0xB9U: s->powered_down = true;               break;
        case 0xABU: s->powered_down = false;              break;
        case 0xC7U:
        case 0x60U:
            if ((s->status & W25_SR_WEL) != 0U) {
                s->pending = W25_OP_ERASE_CHIP;
            }
            break;
        default: break;
        }
        return 0xFFU;
    }

    switch (s->cmd) {
    case 0x05U:                                   /* read status 1 */
        miso = (uint8_t)((s->status & W25_SR_WEL) | (w25_busy(s) ? W25_SR_BUSY : 0U));
        break;
    case 0x9FU: {                                 /* JEDEC ID */
        static const uint8_t jedec[3] = { 0xEFU, 0x40U, 0x15U };
        miso = (idx <= 3U) ? jedec[idx - 1U] : 0xFFU;
        break;
    }
    case 0x90U:                                   /* manufacturer / device */
        if (idx >= 4U) {
            miso = ((idx & 1U) == 0U) ? 0xEFU : 0x14U;
        }
        break;
    case 0xABU:                                   /* release PD / device ID */
        miso = (idx >= 4U) ? 0x14U : 0xFFU;
        break;
    case 0x4BU: {                                 /* unique ID after 4 dummies */
        static const uint8_t uid[8] = { 0xD2U, 0x63U, 0x48U, 0x31U, 0x0FU, 0x55U, 0x2AU, 0x1EU };
        miso = ((idx >= 5U) && (idx < 13U)) ? uid[idx - 5U] : 0xFFU;
        break;
    }
    case 0x03U:                                   /* read data */
    case 0x0BU:                                   /* fast read (1 dummy) */
    case 0x02U:                                   /* page program */
    case 0x20U:                                   /* sector erase 4K */
    case 0xD8U:                                   /* block erase 64K */
        if (idx <= 3U) {
            s->addr = ((s->addr << 8) | mosi) & (W25_SIZE - 1U);
            if (idx == 3U) {
                if ((s->cmd == 0x20U) && ((s->status & W25_SR_WEL) != 0U)) {
                    s->pending = W25_OP_ERASE_4K;
                }
                if ((s->cmd == 0xD8U) && ((s->status & W25_SR_WEL) != 0U)) {
                    s->pending = W25_OP_ERASE_64K;
                }
            }
        } else if (s->cmd == 0x02U) {
            if ((s->status & W25_SR_WEL) != 0U) {
                /* program: bits only go 1 -> 0; address wraps in the page */
                s->mem[s->addr] &= mosi;
                s->addr = (s->addr & ~(W25_PAGE - 1U)) | ((s->addr + 1U) & (W25_PAGE - 1U));
                s->pending = W25_OP_PROGRAM;
            }
        } else if ((s->cmd == 0x0BU) && (idx == 4U)) {
            /* dummy byte */
        } else if ((s->cmd == 0x03U) || (s->cmd == 0x0BU)) {
            miso    = s->mem[s->addr];
            s->addr = (s->addr + 1U) & (W25_SIZE - 1U);
        } else {
            /* erase commands take no data */
        }
        break;
    default:
        break;
    }
    return miso;
}

static sim_spi_dev_t s_dev_w25 = { "W25Q16 SPI flash 2MiB", PA(4), w25_select, w25_transfer, &s_w25, NULL };

/* ============================== SSD1306 =========================== */

#define OLED_W      (128U)
#define OLED_PAGES  (8U)

typedef struct {
    uint8_t  ram[OLED_W * OLED_PAGES];
    bool     first;          /* next byte is the control byte          */
    bool     data_mode;      /* control 0x40: GDDRAM data follows       */
    uint8_t  cmd[3];         /* command being assembled                 */
    uint8_t  cmd_len;
    uint8_t  col, col_start, col_end;
    uint8_t  page, page_start, page_end;
    uint8_t  mux;            /* rows - 1                                 */
    bool     on;
    bool     inverted;
    uint32_t frames;         /* flushes received                        */
} oled_state_t;

static oled_state_t s_oled = { { 0U }, true, false, { 0U }, 0U, 0U, 0U, 127U, 0U, 0U, 7U, 63U,
                               false, false, 0U };

/* number of parameter bytes after each command opcode */
static uint8_t oled_params(uint8_t op)
{
    switch (op) {
    case 0x21U: case 0x22U:                           return 2U;
    case 0x20U: case 0x81U: case 0x8DU: case 0xA8U:
    case 0xD3U: case 0xD5U: case 0xD9U: case 0xDAU:
    case 0xDBU:                                       return 1U;
    default:                                          return 0U;
    }
}

static void oled_command(oled_state_t *o)
{
    switch (o->cmd[0]) {
    case 0x21U: o->col_start = o->cmd[1]; o->col_end = o->cmd[2]; o->col = o->cmd[1]; break;
    case 0x22U: o->page_start = (uint8_t)(o->cmd[1] & 7U); o->page_end = (uint8_t)(o->cmd[2] & 7U);
                o->page = o->page_start; break;
    case 0xA8U: o->mux = o->cmd[1];  break;
    case 0xAEU: o->on = false;       break;
    case 0xAFU: o->on = true;        break;
    case 0xA6U: o->inverted = false; break;
    case 0xA7U: o->inverted = true;  break;
    default:                         break;
    }
}

static bool oled_start(sim_i2c_dev_t *dev, bool read)
{
    oled_state_t *o = (oled_state_t *)dev->ctx;
    o->first   = true;
    o->cmd_len = 0U;
    return !read;                   /* write-only on I2C */
}

static bool oled_write(sim_i2c_dev_t *dev, uint8_t byte)
{
    oled_state_t *o = (oled_state_t *)dev->ctx;
    if (o->first) {
        o->first     = false;
        o->data_mode = (byte & 0x40U) != 0U;
        return true;
    }
    if (o->data_mode) {
        o->ram[((uint32_t)o->page * OLED_W) + o->col] = byte;
        /* horizontal addressing: column, then page, wrap in the window */
        if (o->col >= o->col_end) {
            o->col = o->col_start;
            o->page = (o->page >= o->page_end) ? o->page_start : (uint8_t)(o->page + 1U);
            if (o->page == o->page_start) {
                o->frames++;
            }
        } else {
            o->col++;
        }
        return true;
    }
    o->cmd[o->cmd_len++] = byte;
    if (o->cmd_len > oled_params(o->cmd[0])) {
        oled_command(o);
        o->cmd_len = 0U;
    }
    return true;
}

static sim_i2c_dev_t s_dev_oled = { "SSD1306 OLED 128x64", 0x3CU, oled_start, oled_write, NULL, NULL, &s_oled, NULL };

void sim_oled_show(void)
{
    static uint8_t copy[OLED_W * OLED_PAGES];
    const int16_t  rows = (int16_t)((s_oled.mux >= 63U) ? 64U : (s_oled.mux + 1U));
    gfx_t          view;

    gfx_init(&view, copy, (int16_t)OLED_W, rows);      /* clears copy */
    for (uint32_t i = 0U; i < sizeof(copy); i++) {
        copy[i] = s_oled.inverted ? (uint8_t)~s_oled.ram[i] : s_oled.ram[i];
    }
    io_printf("SSD1306 %s, %lu frame(s)" IO_NEWLINE, s_oled.on ? "on" : "off",
              (unsigned long)s_oled.frames);
    gfx_print_ascii(&view);
}

/* ============================ installation ======================== */

void sim_devices_install_default(void)
{
    static bool installed;
    if (installed) {
        return;
    }
    installed = true;
    for (uint32_t i = 0U; i < EE_SIZE; i++) {
        s_ee.mem[i] = 0xFFU;              /* blank EEPROM */
    }
    sim_i2c_attach(HAL_I2C1, &s_dev_eeprom);
    sim_i2c_attach(HAL_I2C1, &s_dev_lm75);
    sim_i2c_attach(HAL_I2C1, &s_dev_ds1307);
    sim_spi_attach(HAL_SPI1, &s_dev_w25);
    sim_i2c_attach(HAL_I2C1, &s_dev_oled);
}

/**
 * @file    sensors_i2c.c
 * @brief   LM75 and DS1307 drivers (see sensors_i2c.h).
 */
#include "sensors_i2c.h"
#include "codes.h"

#define LM75_REG_TEMP    (0x00U)
#define LM75_REG_CONF    (0x01U)
#define LM75_REG_THYST   (0x02U)
#define LM75_REG_TOS     (0x03U)
#define LM75_CONF_SHDN   (1U << 0)

#define DS_REG_SECONDS   (0x00U)
#define DS_RAM_START     (0x08U)
#define DS_RAM_SIZE      (56U)
#define DS_CH_BIT        (0x80U)   /* clock halt */
#define DS_12H_BIT       (0x40U)

/* ------------------------------ LM75 ------------------------------ */

int32_t lm75_raw_to_c10(uint16_t raw)
{
    /* 9 significant bits, left aligned: arithmetic shift keeps the sign */
    const int32_t half_deg = (int32_t)(int16_t)raw >> 7;
    return half_deg * 5;
}

uint16_t lm75_c10_to_raw(int32_t c10)
{
    const int32_t half_deg = c10 / 5;
    return (uint16_t)((uint32_t)half_deg << 7);
}

hal_status_t lm75_read_c10(hal_i2c_t bus, uint8_t addr, int32_t *c10)
{
    uint8_t buf[2];
    const hal_status_t st = hal_i2c_reg_read(bus, addr, LM75_REG_TEMP, buf, 2U);
    if (st == HAL_OK) {
        *c10 = lm75_raw_to_c10((uint16_t)(((uint16_t)buf[0] << 8) | buf[1]));
    }
    return st;
}

static hal_status_t lm75_write16(hal_i2c_t bus, uint8_t addr, uint8_t reg, uint16_t v)
{
    const uint8_t buf[3] = { reg, (uint8_t)(v >> 8), (uint8_t)v };
    return hal_i2c_write(bus, addr, buf, sizeof(buf));
}

hal_status_t lm75_set_limits(hal_i2c_t bus, uint8_t addr, int32_t hyst_c10, int32_t os_c10)
{
    const hal_status_t st = lm75_write16(bus, addr, LM75_REG_THYST, lm75_c10_to_raw(hyst_c10));
    return (st == HAL_OK) ? lm75_write16(bus, addr, LM75_REG_TOS, lm75_c10_to_raw(os_c10)) : st;
}

hal_status_t lm75_shutdown(hal_i2c_t bus, uint8_t addr, bool shutdown)
{
    return hal_i2c_reg_update8(bus, addr, LM75_REG_CONF, LM75_CONF_SHDN, shutdown ? LM75_CONF_SHDN : 0U);
}

/* ----------------------------- DS1307 ----------------------------- */

static uint8_t from_bcd(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10U) + (v & 0x0FU));
}

hal_status_t ds1307_get(hal_i2c_t bus, datetime_t *dt)
{
    uint8_t r[7];
    const hal_status_t st = hal_i2c_reg_read(bus, DS1307_ADDR, DS_REG_SECONDS, r, sizeof(r));
    if (st != HAL_OK) {
        return st;
    }
    dt->second = from_bcd(r[0] & 0x7FU);
    dt->minute = from_bcd(r[1] & 0x7FU);
    if ((r[2] & DS_12H_BIT) != 0U) {
        /* 12 h mode: bit 5 = PM */
        uint8_t h = from_bcd(r[2] & 0x1FU);
        if (h == 12U) {
            h = 0U;
        }
        dt->hour = (uint8_t)(h + (((r[2] & 0x20U) != 0U) ? 12U : 0U));
    } else {
        dt->hour = from_bcd(r[2] & 0x3FU);
    }
    dt->weekday = (uint8_t)((r[3] & 0x07U) - 1U);
    dt->day     = from_bcd(r[4] & 0x3FU);
    dt->month   = from_bcd(r[5] & 0x1FU);
    dt->year    = (uint16_t)(2000U + from_bcd(r[6]));
    return dt_is_valid(dt) ? HAL_OK : HAL_ERROR;
}

hal_status_t ds1307_set(hal_i2c_t bus, const datetime_t *dt)
{
    if (!dt_is_valid(dt) || (dt->year < 2000U) || (dt->year > 2099U)) {
        return HAL_EINVAL;
    }
    uint32_t t;
    datetime_t norm;
    (void)dt_to_unix(dt, &t);
    dt_from_unix(t, &norm);              /* computes the weekday */
    const uint8_t buf[8] = {
        DS_REG_SECONDS,
        (uint8_t)codes_bin_to_bcd(norm.second),     /* CH = 0: oscillator on */
        (uint8_t)codes_bin_to_bcd(norm.minute),
        (uint8_t)codes_bin_to_bcd(norm.hour),       /* 24 h mode */
        (uint8_t)(norm.weekday + 1U),
        (uint8_t)codes_bin_to_bcd(norm.day),
        (uint8_t)codes_bin_to_bcd(norm.month),
        (uint8_t)codes_bin_to_bcd(norm.year - 2000U),
    };
    return hal_i2c_write(bus, DS1307_ADDR, buf, sizeof(buf));
}

hal_status_t ds1307_ram_read(hal_i2c_t bus, uint8_t offset, uint8_t *data, size_t len)
{
    if ((offset + len) > DS_RAM_SIZE) {
        return HAL_EINVAL;
    }
    return hal_i2c_reg_read(bus, DS1307_ADDR, (uint8_t)(DS_RAM_START + offset), data, len);
}

hal_status_t ds1307_ram_write(hal_i2c_t bus, uint8_t offset, const uint8_t *data, size_t len)
{
    uint8_t buf[1U + DS_RAM_SIZE];
    if ((offset + len) > DS_RAM_SIZE) {
        return HAL_EINVAL;
    }
    buf[0] = (uint8_t)(DS_RAM_START + offset);
    for (size_t i = 0U; i < len; i++) {
        buf[1U + i] = data[i];
    }
    return hal_i2c_write(bus, DS1307_ADDR, buf, len + 1U);
}

/**
 * @file    sensors_i2c.h
 * @brief   Small I2C chips seen in every lab kit:
 *          LM75 temperature sensor and DS1307 / DS3231 RTC.
 */
#ifndef SENSORS_I2C_H
#define SENSORS_I2C_H

#include "hal_i2c.h"
#include "datetime.h"

#define LM75_ADDR_DEFAULT    (0x48U)
#define DS1307_ADDR          (0x68U)

/* ------------------------------ LM75 ------------------------------ */
/** Temperature in 0.1 C (9-bit chip: 0.5 C steps). */
hal_status_t lm75_read_c10(hal_i2c_t bus, uint8_t addr, int32_t *c10);
/** Over-temperature output (OS pin) thresholds, in 0.1 C. */
hal_status_t lm75_set_limits(hal_i2c_t bus, uint8_t addr, int32_t hyst_c10, int32_t os_c10);
hal_status_t lm75_shutdown(hal_i2c_t bus, uint8_t addr, bool shutdown);
/** Register value <-> 0.1 C (portable, unit-tested). */
int32_t      lm75_raw_to_c10(uint16_t raw);
uint16_t     lm75_c10_to_raw(int32_t c10);

/* ----------------------------- DS1307 ----------------------------- */
hal_status_t ds1307_get(hal_i2c_t bus, datetime_t *dt);
hal_status_t ds1307_set(hal_i2c_t bus, const datetime_t *dt);
/** 56 bytes of battery-backed RAM at 0x08..0x3F. */
hal_status_t ds1307_ram_read(hal_i2c_t bus, uint8_t offset, uint8_t *data, size_t len);
hal_status_t ds1307_ram_write(hal_i2c_t bus, uint8_t offset, const uint8_t *data, size_t len);

#endif /* SENSORS_I2C_H */

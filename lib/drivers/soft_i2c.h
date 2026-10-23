/**
 * @file    soft_i2c.h
 * @brief   Bit-banged I2C master on any two GPIOs (open drain).
 *
 * Useful when the hardware I2C pins are taken, or to understand the
 * protocol: every START, bit, ACK and STOP is visible in the code.
 * Clock stretching is supported (the slave can hold SCL low).
 * Speed is set by the half-period delay (5 us ~ 100 kHz).
 */
#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include "hal_types.h"
#include "hal_gpio.h"

typedef struct {
    hal_pin_t scl;
    hal_pin_t sda;
    uint32_t  half_period_us;
} soft_i2c_t;

void         soft_i2c_init(soft_i2c_t *bus, hal_pin_t scl, hal_pin_t sda, uint32_t half_period_us);
/** Free a stuck bus: 9 clocks then STOP (slave held SDA after a reset). */
void         soft_i2c_recover(soft_i2c_t *bus);
void         soft_i2c_start(soft_i2c_t *bus);
void         soft_i2c_stop(soft_i2c_t *bus);
/** Returns true if the slave acknowledged. */
bool         soft_i2c_write_byte(soft_i2c_t *bus, uint8_t byte);
uint8_t      soft_i2c_read_byte(soft_i2c_t *bus, bool ack);

hal_status_t soft_i2c_write(soft_i2c_t *bus, uint8_t addr, const uint8_t *data, size_t len);
hal_status_t soft_i2c_read(soft_i2c_t *bus, uint8_t addr, uint8_t *data, size_t len);
hal_status_t soft_i2c_write_read(soft_i2c_t *bus, uint8_t addr, const uint8_t *w, size_t wlen,
                                 uint8_t *r, size_t rlen);
bool         soft_i2c_probe(soft_i2c_t *bus, uint8_t addr);

#endif /* SOFT_I2C_H */

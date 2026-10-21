/**
 * @file    soft_i2c.c
 * @brief   Bit-banged I2C (see soft_i2c.h).
 *
 * Open-drain convention: writing 1 releases the line (the pull-up
 * makes it high), writing 0 pulls it low. We never drive a line high.
 */
#include "soft_i2c.h"
#include "hal_time.h"

#define STRETCH_TIMEOUT_US  (1000U)

static void delay(const soft_i2c_t *bus)
{
    hal_delay_us(bus->half_period_us);
}

static void sda(const soft_i2c_t *bus, bool level) { hal_gpio_write(bus->sda, level); }
static bool sda_read(const soft_i2c_t *bus)         { return hal_gpio_read(bus->sda); }

static void scl_high(const soft_i2c_t *bus)
{
    hal_gpio_set(bus->scl);
    /* clock stretching: wait while the slave keeps SCL low */
    const uint32_t t0 = hal_time_us();
    while (!hal_gpio_read(bus->scl) && ((hal_time_us() - t0) < STRETCH_TIMEOUT_US)) {
    }
}

static void scl_low(const soft_i2c_t *bus)
{
    hal_gpio_clear(bus->scl);
}

void soft_i2c_init(soft_i2c_t *bus, hal_pin_t scl, hal_pin_t sda_pin, uint32_t half_period_us)
{
    bus->scl            = scl;
    bus->sda            = sda_pin;
    bus->half_period_us = (half_period_us == 0U) ? 1U : half_period_us;
    hal_gpio_set(scl);
    hal_gpio_set(sda_pin);
    hal_gpio_mode(scl, HAL_GPIO_OUTPUT_OD);
    hal_gpio_mode(sda_pin, HAL_GPIO_OUTPUT_OD);
}

void soft_i2c_recover(soft_i2c_t *bus)
{
    sda(bus, true);
    for (uint32_t i = 0U; i < 9U; i++) {
        scl_low(bus);
        delay(bus);
        scl_high(bus);
        delay(bus);
    }
    soft_i2c_stop(bus);
}

void soft_i2c_start(soft_i2c_t *bus)
{
    /* START: SDA falls while SCL is high (works as repeated START too) */
    sda(bus, true);
    scl_high(bus);
    delay(bus);
    sda(bus, false);
    delay(bus);
    scl_low(bus);
}

void soft_i2c_stop(soft_i2c_t *bus)
{
    /* STOP: SDA rises while SCL is high */
    sda(bus, false);
    delay(bus);
    scl_high(bus);
    delay(bus);
    sda(bus, true);
    delay(bus);
}

bool soft_i2c_write_byte(soft_i2c_t *bus, uint8_t byte)
{
    for (uint32_t mask = 0x80U; mask != 0U; mask >>= 1) {
        sda(bus, (byte & mask) != 0U);    /* data changes while SCL low */
        delay(bus);
        scl_high(bus);                    /* slave samples on SCL high */
        delay(bus);
        scl_low(bus);
    }
    /* 9th clock: release SDA, slave pulls it low to ACK */
    sda(bus, true);
    delay(bus);
    scl_high(bus);
    const bool ack = !sda_read(bus);
    delay(bus);
    scl_low(bus);
    return ack;
}

uint8_t soft_i2c_read_byte(soft_i2c_t *bus, bool ack)
{
    uint8_t byte = 0U;
    sda(bus, true);                       /* release SDA for the slave */
    for (uint32_t i = 0U; i < 8U; i++) {
        delay(bus);
        scl_high(bus);
        byte = (uint8_t)((byte << 1) | (sda_read(bus) ? 1U : 0U));
        delay(bus);
        scl_low(bus);
    }
    sda(bus, !ack);                       /* ACK = 0, NACK on last byte */
    delay(bus);
    scl_high(bus);
    delay(bus);
    scl_low(bus);
    sda(bus, true);
    return byte;
}

static hal_status_t address(soft_i2c_t *bus, uint8_t addr, bool read)
{
    soft_i2c_start(bus);
    if (!soft_i2c_write_byte(bus, (uint8_t)((addr << 1) | (read ? 1U : 0U)))) {
        soft_i2c_stop(bus);
        return HAL_ENACK;
    }
    return HAL_OK;
}

hal_status_t soft_i2c_write(soft_i2c_t *bus, uint8_t addr, const uint8_t *data, size_t len)
{
    hal_status_t st = address(bus, addr, false);
    for (size_t i = 0U; (st == HAL_OK) && (i < len); i++) {
        if (!soft_i2c_write_byte(bus, data[i])) {
            st = HAL_ENACK;
        }
    }
    soft_i2c_stop(bus);
    return st;
}

hal_status_t soft_i2c_read(soft_i2c_t *bus, uint8_t addr, uint8_t *data, size_t len)
{
    const hal_status_t st = address(bus, addr, true);
    if (st != HAL_OK) {
        return st;
    }
    for (size_t i = 0U; i < len; i++) {
        data[i] = soft_i2c_read_byte(bus, i < (len - 1U));
    }
    soft_i2c_stop(bus);
    return HAL_OK;
}

hal_status_t soft_i2c_write_read(soft_i2c_t *bus, uint8_t addr, const uint8_t *w, size_t wlen,
                                 uint8_t *r, size_t rlen)
{
    hal_status_t st = address(bus, addr, false);
    for (size_t i = 0U; (st == HAL_OK) && (i < wlen); i++) {
        if (!soft_i2c_write_byte(bus, w[i])) {
            st = HAL_ENACK;
            soft_i2c_stop(bus);
        }
    }
    if (st != HAL_OK) {
        return st;
    }
    st = address(bus, addr, true);        /* repeated START */
    if (st != HAL_OK) {
        return st;
    }
    for (size_t i = 0U; i < rlen; i++) {
        r[i] = soft_i2c_read_byte(bus, i < (rlen - 1U));
    }
    soft_i2c_stop(bus);
    return HAL_OK;
}

bool soft_i2c_probe(soft_i2c_t *bus, uint8_t addr)
{
    const bool ok = address(bus, addr, false) == HAL_OK;
    if (ok) {
        soft_i2c_stop(bus);
    }
    return ok;
}

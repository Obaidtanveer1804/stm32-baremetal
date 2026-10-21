/**
 * @file    hal_i2c.h
 * @brief   I2C master with timeouts (never hangs on a missing device).
 *
 *   I2C1: SCL PB6,  SDA PB7   (conflicts with PWM T4C1/T4C2)
 *   I2C2: SCL PB10, SDA PB11  (conflicts with UART3)
 *
 * Addresses are 7-bit (0x50 for a 24C02, not 0xA0).
 * For any-pin I2C see drivers/soft_i2c.h (bit-banged).
 */
#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "hal_types.h"

typedef enum {
    HAL_I2C1 = 0,
    HAL_I2C2,
    HAL_I2C_COUNT
} hal_i2c_t;

#define HAL_I2C_TIMEOUT_MS  (10U)

hal_status_t hal_i2c_init(hal_i2c_t bus, uint32_t speed_hz);
hal_status_t hal_i2c_write(hal_i2c_t bus, uint8_t addr, const uint8_t *data, size_t len);
hal_status_t hal_i2c_read(hal_i2c_t bus, uint8_t addr, uint8_t *data, size_t len);
/** Write then repeated-START read (typical "read register"). */
hal_status_t hal_i2c_write_read(hal_i2c_t bus, uint8_t addr,
                                const uint8_t *wdata, size_t wlen,
                                uint8_t *rdata, size_t rlen);
/** true if a device acknowledges @p addr. */
bool         hal_i2c_probe(hal_i2c_t bus, uint8_t addr);

/* Register helpers (portable, built on the functions above). */
hal_status_t hal_i2c_reg_write8(hal_i2c_t bus, uint8_t addr, uint8_t reg, uint8_t value);
hal_status_t hal_i2c_reg_read8(hal_i2c_t bus, uint8_t addr, uint8_t reg, uint8_t *value);
hal_status_t hal_i2c_reg_read(hal_i2c_t bus, uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
/** Read-modify-write of the bits in @p mask. */
hal_status_t hal_i2c_reg_update8(hal_i2c_t bus, uint8_t addr, uint8_t reg,
                                 uint8_t mask, uint8_t value);

#endif /* HAL_I2C_H */

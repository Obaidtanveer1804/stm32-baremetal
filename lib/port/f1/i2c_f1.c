/**
 * @file    i2c_f1.c
 * @brief   hal_i2c on the F1 I2C peripheral, register level, with a
 *          timeout on every wait (the libopencm3 helpers can hang
 *          forever when a device is missing or the bus is stuck).
 *
 * Read sequences follow RM0008 26.3.3 "method 2" for N = 1, 2, > 2:
 * the ACK / STOP bits must be programmed at precise moments because
 * the peripheral already clocks the next byte in.
 */
#include "hal_i2c.h"
#include "hal_gpio.h"
#include "hal_time.h"
#include "hal_irq.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>

typedef struct {
    uint32_t              base;
    enum rcc_periph_clken clock;
    enum rcc_periph_rst   reset;
    hal_pin_t             scl;
    hal_pin_t             sda;
} i2c_hw_t;

static const i2c_hw_t i2c_hw[HAL_I2C_COUNT] = {
    { I2C1, RCC_I2C1, RST_I2C1, PB(6),  PB(7)  },
    { I2C2, RCC_I2C2, RST_I2C2, PB(10), PB(11) },
};

#define I2C_DIR_WRITE  (0U)
#define I2C_DIR_READ   (1U)

/* Wait until (SR1 & flag) != 0. Aborts on NACK or timeout. */
static hal_status_t wait_sr1(uint32_t base, uint32_t flag)
{
    const uint32_t start = hal_time_ms();
    while ((I2C_SR1(base) & flag) == 0U) {
        if ((I2C_SR1(base) & I2C_SR1_AF) != 0U) {
            I2C_SR1(base) &= ~I2C_SR1_AF;
            return HAL_ENACK;
        }
        if ((hal_time_ms() - start) > HAL_I2C_TIMEOUT_MS) {
            return HAL_ETIMEOUT;
        }
    }
    return HAL_OK;
}

static hal_status_t wait_not_busy(uint32_t base)
{
    const uint32_t start = hal_time_ms();
    while ((I2C_SR2(base) & I2C_SR2_BUSY) != 0U) {
        if ((hal_time_ms() - start) > HAL_I2C_TIMEOUT_MS) {
            return HAL_EBUSY;
        }
    }
    return HAL_OK;
}

static void clear_addr(uint32_t base)
{
    /* ADDR is cleared by reading SR1 then SR2 */
    volatile uint32_t sr;
    sr = I2C_SR1(base);
    sr = I2C_SR2(base);
    (void)sr;
}

static void stop(uint32_t base)
{
    I2C_CR1(base) |= I2C_CR1_STOP;
}

/* START + address. On success ADDR is set but NOT yet cleared. */
static hal_status_t start_addr(uint32_t base, uint8_t addr, uint32_t dir)
{
    hal_status_t st;

    I2C_CR1(base) |= I2C_CR1_START;
    st = wait_sr1(base, I2C_SR1_SB);
    if (st != HAL_OK) {
        stop(base);
        return st;
    }
    I2C_DR(base) = (uint32_t)((addr << 1) | dir);
    st = wait_sr1(base, I2C_SR1_ADDR);
    if (st != HAL_OK) {
        stop(base);
    }
    return st;
}

static hal_status_t write_bytes(uint32_t base, const uint8_t *data, size_t len)
{
    for (size_t i = 0U; i < len; i++) {
        const hal_status_t st = wait_sr1(base, I2C_SR1_TxE);
        if (st != HAL_OK) {
            stop(base);
            return st;
        }
        I2C_DR(base) = data[i];
    }
    const hal_status_t st = wait_sr1(base, I2C_SR1_BTF);
    if (st != HAL_OK) {
        stop(base);
    }
    return st;
}

static hal_status_t read_bytes(uint32_t base, uint8_t addr, uint8_t *data, size_t len)
{
    hal_status_t st = start_addr(base, addr, I2C_DIR_READ);
    if (st != HAL_OK) {
        return st;
    }

    if (len == 1U) {
        I2C_CR1(base) &= ~I2C_CR1_ACK;
        /* ADDR clear + STOP must not be separated by an interrupt */
        const uint32_t irq = hal_irq_save();
        clear_addr(base);
        stop(base);
        hal_irq_restore(irq);
        st = wait_sr1(base, I2C_SR1_RxNE);
        if (st == HAL_OK) {
            data[0] = (uint8_t)I2C_DR(base);
        }
        return st;
    }

    if (len == 2U) {
        /* RM0008 26.3.3: ACK = 0 and POS = 1 *before* clearing ADDR, so
         * the NACK is sent after the second byte */
        I2C_CR1(base) &= ~I2C_CR1_ACK;
        I2C_CR1(base) |= I2C_CR1_POS;
        clear_addr(base);
        st = wait_sr1(base, I2C_SR1_BTF);      /* both bytes received */
        if (st == HAL_OK) {
            const uint32_t irq2 = hal_irq_save();
            stop(base);
            data[0] = (uint8_t)I2C_DR(base);
            hal_irq_restore(irq2);
            data[1] = (uint8_t)I2C_DR(base);
        } else {
            stop(base);
        }
        I2C_CR1(base) &= ~I2C_CR1_POS;
        return st;
    }

    /* len > 2: ACK every byte until the last three */
    I2C_CR1(base) |= I2C_CR1_ACK;
    clear_addr(base);
    size_t i = 0U;
    while ((len - i) > 3U) {
        st = wait_sr1(base, I2C_SR1_RxNE);
        if (st != HAL_OK) {
            stop(base);
            return st;
        }
        data[i++] = (uint8_t)I2C_DR(base);
    }
    /* N-2 in DR, N-1 in the shift register */
    st = wait_sr1(base, I2C_SR1_BTF);
    if (st != HAL_OK) {
        stop(base);
        return st;
    }
    I2C_CR1(base) &= ~I2C_CR1_ACK;
    data[i++] = (uint8_t)I2C_DR(base);         /* byte N-2 */
    st = wait_sr1(base, I2C_SR1_BTF);
    if (st != HAL_OK) {
        stop(base);
        return st;
    }
    const uint32_t irq = hal_irq_save();
    stop(base);
    data[i++] = (uint8_t)I2C_DR(base);         /* byte N-1 */
    hal_irq_restore(irq);
    st = wait_sr1(base, I2C_SR1_RxNE);
    if (st == HAL_OK) {
        data[i] = (uint8_t)I2C_DR(base);       /* byte N */
    }
    return st;
}

hal_status_t hal_i2c_init(hal_i2c_t bus, uint32_t speed_hz)
{
    if (bus >= HAL_I2C_COUNT) {
        return HAL_EINVAL;
    }
    const i2c_hw_t *hw  = &i2c_hw[bus];
    const uint32_t  mhz = rcc_apb1_frequency / 1000000U;

    rcc_periph_clock_enable(hw->clock);
    rcc_periph_reset_pulse(hw->reset);
    hal_gpio_mode(hw->scl, HAL_GPIO_AF_OD);
    hal_gpio_mode(hw->sda, HAL_GPIO_AF_OD);

    i2c_peripheral_disable(hw->base);
    i2c_set_speed(hw->base, (speed_hz > 100000U) ? i2c_speed_fm_400k : i2c_speed_sm_100k, mhz);
    i2c_peripheral_enable(hw->base);

    /* STM32F1 errata sheet, "I2C analog filter may provide wrong value,
     * locking BUSY flag": a reset in the middle of a transfer can leave
     * BUSY set. SWRST clears the peripheral state; if a slave still holds
     * SDA low, free the bus first with soft_i2c_recover() (9 clocks). */
    if ((I2C_SR2(hw->base) & I2C_SR2_BUSY) != 0U) {
        I2C_CR1(hw->base) |= I2C_CR1_SWRST;
        I2C_CR1(hw->base) &= ~I2C_CR1_SWRST;
        i2c_set_speed(hw->base, (speed_hz > 100000U) ? i2c_speed_fm_400k : i2c_speed_sm_100k, mhz);
        i2c_peripheral_enable(hw->base);
    }
    return HAL_OK;
}

hal_status_t hal_i2c_write(hal_i2c_t bus, uint8_t addr, const uint8_t *data, size_t len)
{
    if (bus >= HAL_I2C_COUNT) {
        return HAL_EINVAL;
    }
    const uint32_t base = i2c_hw[bus].base;
    hal_status_t   st   = wait_not_busy(base);
    if (st == HAL_OK) {
        st = start_addr(base, addr, I2C_DIR_WRITE);
    }
    if (st == HAL_OK) {
        clear_addr(base);
        st = write_bytes(base, data, len);
        if (st == HAL_OK) {
            stop(base);
        }
    }
    return st;
}

hal_status_t hal_i2c_read(hal_i2c_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    if ((bus >= HAL_I2C_COUNT) || (len == 0U)) {
        return HAL_EINVAL;
    }
    const uint32_t base = i2c_hw[bus].base;
    const hal_status_t st = wait_not_busy(base);
    return (st == HAL_OK) ? read_bytes(base, addr, data, len) : st;
}

hal_status_t hal_i2c_write_read(hal_i2c_t bus, uint8_t addr,
                                const uint8_t *wdata, size_t wlen,
                                uint8_t *rdata, size_t rlen)
{
    if ((bus >= HAL_I2C_COUNT) || (rlen == 0U)) {
        return HAL_EINVAL;
    }
    const uint32_t base = i2c_hw[bus].base;
    hal_status_t   st   = wait_not_busy(base);
    if (st == HAL_OK) {
        st = start_addr(base, addr, I2C_DIR_WRITE);
    }
    if (st == HAL_OK) {
        clear_addr(base);
        st = write_bytes(base, wdata, wlen);
    }
    /* no STOP: the next START is a repeated start */
    return (st == HAL_OK) ? read_bytes(base, addr, rdata, rlen) : st;
}

bool hal_i2c_probe(hal_i2c_t bus, uint8_t addr)
{
    if (bus >= HAL_I2C_COUNT) {
        return false;
    }
    const uint32_t base = i2c_hw[bus].base;
    if (wait_not_busy(base) != HAL_OK) {
        return false;
    }
    const hal_status_t st = start_addr(base, addr, I2C_DIR_WRITE);
    if (st == HAL_OK) {
        clear_addr(base);
        stop(base);
    }
    return st == HAL_OK;
}

/**
 * @file    shiftreg.c
 * @brief   74HC595 / 74HC165 (see shiftreg.h).
 */
#include "shiftreg.h"

void shift_out(hal_pin_t data, hal_pin_t clk, uint8_t value, bool msb_first)
{
    for (uint32_t i = 0U; i < 8U; i++) {
        const uint32_t bit = msb_first ? (7U - i) : i;
        hal_gpio_write(data, ((value >> bit) & 1U) != 0U);
        hal_gpio_set(clk);           /* 595 samples SER on the rising edge */
        hal_gpio_clear(clk);
    }
}

uint8_t shift_in(hal_pin_t data, hal_pin_t clk, bool msb_first)
{
    uint8_t value = 0U;
    for (uint32_t i = 0U; i < 8U; i++) {
        const uint32_t bit = msb_first ? (7U - i) : i;
        if (hal_gpio_read(data)) {
            value |= (uint8_t)(1U << bit);
        }
        hal_gpio_set(clk);           /* next bit appears on QH */
        hal_gpio_clear(clk);
    }
    return value;
}

void shiftreg595_init(shiftreg_t *sr, hal_pin_t data, hal_pin_t clk, hal_pin_t latch, uint8_t chips)
{
    sr->data  = data;
    sr->clk   = clk;
    sr->latch = latch;
    sr->chips = (chips > SHIFTREG_MAX_CHIPS) ? (uint8_t)SHIFTREG_MAX_CHIPS : ((chips == 0U) ? 1U : chips);
    hal_gpio_mode(data, HAL_GPIO_OUTPUT_FAST);
    hal_gpio_mode(clk, HAL_GPIO_OUTPUT_FAST);
    hal_gpio_mode(latch, HAL_GPIO_OUTPUT_FAST);
    hal_gpio_clear(clk);
    hal_gpio_clear(latch);
    shiftreg595_write(sr, 0U);
}

void shiftreg595_update(shiftreg_t *sr)
{
    /* the last chip of the chain must be shifted first */
    for (uint32_t i = sr->chips; i > 0U; i--) {
        shift_out(sr->data, sr->clk, sr->shadow[i - 1U], true);
    }
    hal_gpio_set(sr->latch);         /* RCLK rising edge copies to outputs */
    hal_gpio_clear(sr->latch);
}

void shiftreg595_write(shiftreg_t *sr, uint32_t value)
{
    for (uint32_t i = 0U; i < sr->chips; i++) {
        sr->shadow[i] = (uint8_t)(value >> (8U * i));
    }
    shiftreg595_update(sr);
}

void shiftreg595_write_bit(shiftreg_t *sr, uint32_t bit, bool level)
{
    const uint32_t chip = bit / 8U;
    const uint8_t  mask = (uint8_t)(1U << (bit % 8U));
    if (chip >= sr->chips) {
        return;
    }
    if (level) {
        sr->shadow[chip] |= mask;
    } else {
        sr->shadow[chip] &= (uint8_t)~mask;
    }
    shiftreg595_update(sr);
}

uint32_t shiftreg595_get(const shiftreg_t *sr)
{
    uint32_t v = 0U;
    for (uint32_t i = 0U; i < sr->chips; i++) {
        v |= (uint32_t)sr->shadow[i] << (8U * i);
    }
    return v;
}

void shiftreg165_init(shiftreg_t *sr, hal_pin_t data, hal_pin_t clk, hal_pin_t load, uint8_t chips)
{
    sr->data  = data;
    sr->clk   = clk;
    sr->latch = load;
    sr->chips = (chips > SHIFTREG_MAX_CHIPS) ? (uint8_t)SHIFTREG_MAX_CHIPS : ((chips == 0U) ? 1U : chips);
    hal_gpio_mode(data, HAL_GPIO_INPUT);
    hal_gpio_mode(clk, HAL_GPIO_OUTPUT_FAST);
    hal_gpio_mode(load, HAL_GPIO_OUTPUT_FAST);
    hal_gpio_clear(clk);
    hal_gpio_set(load);
}

uint32_t shiftreg165_read(shiftreg_t *sr)
{
    uint32_t v = 0U;
    hal_gpio_clear(sr->latch);       /* SH/LD low: load the 8 inputs */
    hal_gpio_set(sr->latch);         /* back to shift mode */
    for (uint32_t i = 0U; i < sr->chips; i++) {
        /* QH (input H) comes out first */
        v |= (uint32_t)shift_in(sr->data, sr->clk, true) << (8U * i);
    }
    return v;
}

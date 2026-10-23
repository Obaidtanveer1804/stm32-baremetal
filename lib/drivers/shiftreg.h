/**
 * @file    shiftreg.h
 * @brief   74HC595 output shift registers (daisy-chained) and 74HC165
 *          input shift registers, bit-banged on 3 GPIOs.
 *
 *  595: DATA -> SER, CLK -> SRCLK, LATCH -> RCLK. 8 extra outputs per chip.
 *  165: DATA <- QH, CLK -> CLK, LATCH -> SH/LD (low = load). 8 inputs per chip.
 *
 * The driver keeps a shadow copy of the outputs, so single bits can
 * be changed like a GPIO port: shiftreg_write_bit(&sr, 13, true).
 */
#ifndef SHIFTREG_H
#define SHIFTREG_H

#include "hal_gpio.h"

#define SHIFTREG_MAX_CHIPS  (4U)

typedef struct {
    hal_pin_t data;
    hal_pin_t clk;
    hal_pin_t latch;
    uint8_t   chips;
    uint8_t   shadow[SHIFTREG_MAX_CHIPS];   /**< [0] = chip nearest the MCU */
} shiftreg_t;

void     shiftreg595_init(shiftreg_t *sr, hal_pin_t data, hal_pin_t clk, hal_pin_t latch, uint8_t chips);
/** Send the shadow buffer and latch it to the outputs. */
void     shiftreg595_update(shiftreg_t *sr);
void     shiftreg595_write(shiftreg_t *sr, uint32_t value);     /**< all bits at once */
void     shiftreg595_write_bit(shiftreg_t *sr, uint32_t bit, bool level);
uint32_t shiftreg595_get(const shiftreg_t *sr);

void     shiftreg165_init(shiftreg_t *sr, hal_pin_t data, hal_pin_t clk, hal_pin_t load, uint8_t chips);
/** Parallel load then shift in: bit 0 = input A of the first chip. */
uint32_t shiftreg165_read(shiftreg_t *sr);

/** Shift one byte out MSB first (generic helper, like Arduino shiftOut). */
void     shift_out(hal_pin_t data, hal_pin_t clk, uint8_t value, bool msb_first);
uint8_t  shift_in(hal_pin_t data, hal_pin_t clk, bool msb_first);

#endif /* SHIFTREG_H */

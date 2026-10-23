/**
 * @file    counter.h
 * @brief   Generic wrapping counters + display in several bases.
 *
 * Generalisation of the TP2 binary / hex / decimal counters: the
 * counting logic is pure state, the display only goes through io.h,
 * so the module runs identically on the PC and the STM32.
 */
#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    COUNTER_MODE_WRAP = 0,  /**< max -> min (and min -> max going down) */
    COUNTER_MODE_SATURATE,  /**< stop at the limits                     */
    COUNTER_MODE_BOUNCE     /**< reverse direction at the limits        */
} counter_mode_t;

typedef struct {
    int32_t        value;
    int32_t        min;
    int32_t        max;
    int32_t        step;     /**< signed: negative counts down          */
    counter_mode_t mode;
    uint32_t       wraps;    /**< number of wraps / bounces so far      */
} counter_t;

void    counter_init(counter_t *c, int32_t min, int32_t max, int32_t step,
                     counter_mode_t mode);
/** Advance by one step; returns true when a wrap/bounce/limit happened. */
bool    counter_step(counter_t *c);
void    counter_set(counter_t *c, int32_t value);
void    counter_reset(counter_t *c);
int32_t counter_get(const counter_t *c);

/* --- display helpers, same output format as TP2 ------------------- */
void counter_print_decimal(uint32_t value, uint32_t digits);
void counter_print_binary(uint32_t value, uint32_t bits);
void counter_print_hex(uint32_t value, uint32_t digits);
void counter_print_bcd(uint32_t value, uint32_t digits);
void counter_print_gray(uint32_t value, uint32_t bits);
/** One line with every representation of @p value. */
void counter_print_all(uint32_t value, uint32_t bits);

/* --- 32-bit software counters on top of 16-bit hardware timers ---- */

/**
 * @brief  Extend a free-running 16-bit hardware counter to 32 bits.
 *         Call at least once per 16-bit overflow period.
 */
typedef struct {
    uint16_t last;
    uint32_t extended;
} counter_ext16_t;

void     counter_ext16_init(counter_ext16_t *e, uint16_t hw_now);
uint32_t counter_ext16_update(counter_ext16_t *e, uint16_t hw_now);

#endif /* COUNTER_H */

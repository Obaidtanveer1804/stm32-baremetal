/**
 * @file    encoder.h
 * @brief   Quadrature rotary encoder (KY-040, motor encoders), decoded
 *          with a 16-entry state table: immune to contact bounce,
 *          counts every edge (x4) and detects direction.
 *
 * Call encoder_update() from a timer interrupt (1 kHz is plenty for a
 * hand-turned knob) or from the EXTI callbacks of both pins.
 */
#ifndef ENCODER_H
#define ENCODER_H

#include "hal_gpio.h"

typedef struct {
    hal_pin_t         a;
    hal_pin_t         b;
    uint8_t           state;      /* previous AB (2 bits)        */
    volatile int32_t  position;   /* in edges (4 per detent)     */
    uint8_t           steps_per_detent;
} encoder_t;

void    encoder_init(encoder_t *e, hal_pin_t a, hal_pin_t b, uint8_t steps_per_detent);
/** Sample the pins; returns -1, 0 or +1 (edge direction). */
int32_t encoder_update(encoder_t *e);
/** Position in detents (clicks of the knob). */
int32_t encoder_get(const encoder_t *e);
void    encoder_set(encoder_t *e, int32_t detents);
/** Detents moved since the last call (for menus). */
int32_t encoder_delta(encoder_t *e);
/** Pure decoder (portable, unit-tested): -1, 0, +1 for a transition. */
int32_t encoder_decode(uint8_t prev_ab, uint8_t new_ab);

#endif /* ENCODER_H */

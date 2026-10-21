/**
 * @file    encoder.c
 * @brief   Quadrature decoder (see encoder.h).
 */
#include "encoder.h"

/*
 * Index = (prev AB << 2) | new AB. Valid Gray-code steps give +-1,
 * no change gives 0, impossible jumps (both bits changed: a missed
 * sample or bounce) give 0 too, so noise cannot accumulate.
 *   CW sequence: 00 -> 01 -> 11 -> 10 -> 00
 */
static const int8_t quad_table[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

int32_t encoder_decode(uint8_t prev_ab, uint8_t new_ab)
{
    return quad_table[((prev_ab & 3U) << 2) | (new_ab & 3U)];
}

static uint8_t read_ab(const encoder_t *e)
{
    return (uint8_t)((hal_gpio_read(e->a) ? 2U : 0U) | (hal_gpio_read(e->b) ? 1U : 0U));
}

void encoder_init(encoder_t *e, hal_pin_t a, hal_pin_t b, uint8_t steps_per_detent)
{
    e->a                = a;
    e->b                = b;
    e->steps_per_detent = (steps_per_detent == 0U) ? 4U : steps_per_detent;
    hal_gpio_mode(a, HAL_GPIO_INPUT_PULLUP);   /* KY-040: switches to GND */
    hal_gpio_mode(b, HAL_GPIO_INPUT_PULLUP);
    e->state    = read_ab(e);
    e->position = 0;
}

int32_t encoder_update(encoder_t *e)
{
    const uint8_t ab  = read_ab(e);
    const int32_t dir = encoder_decode(e->state, ab);
    e->state     = ab;
    e->position += dir;
    return dir;
}

int32_t encoder_get(const encoder_t *e)
{
    /* round toward zero symmetric for negative positions */
    const int32_t p = e->position;
    const int32_t s = e->steps_per_detent;
    return (p >= 0) ? (p / s) : -((-p) / s);
}

void encoder_set(encoder_t *e, int32_t detents)
{
    e->position = detents * (int32_t)e->steps_per_detent;
}

int32_t encoder_delta(encoder_t *e)
{
    const int32_t s = e->steps_per_detent;
    int32_t       d = 0;
    /* consume whole detents only, keep the remainder */
    while (e->position >= s) {
        e->position -= s;
        d++;
    }
    while (e->position <= -s) {
        e->position += s;
        d--;
    }
    return d;
}

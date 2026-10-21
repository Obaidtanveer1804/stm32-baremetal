/**
 * @file    button.h
 * @brief   Push button on a GPIO: debounced, with click / double /
 *          long press events (core/debounce.h on a real pin).
 */
#ifndef BUTTON_H
#define BUTTON_H

#include "hal_gpio.h"
#include "debounce.h"

typedef struct {
    hal_pin_t pin;
    bool      active_high;
    btn_t     detector;
} button_t;

/** Configures the pull resistor opposite to the active level. */
void        button_init(button_t *b, hal_pin_t pin, bool active_high, const btn_config_t *cfg);
bool        button_is_pressed(const button_t *b);    /**< raw level   */
/** Call every 1..10 ms. Returns the gesture event, if any. */
btn_event_t button_update(button_t *b, uint32_t now_ms);

#endif /* BUTTON_H */

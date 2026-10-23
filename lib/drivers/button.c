/**
 * @file    button.c
 * @brief   GPIO push button (see button.h).
 */
#include "button.h"

void button_init(button_t *b, hal_pin_t pin, bool active_high, const btn_config_t *cfg)
{
    b->pin         = pin;
    b->active_high = active_high;
    hal_gpio_mode(pin, active_high ? HAL_GPIO_INPUT_PULLDOWN : HAL_GPIO_INPUT_PULLUP);
    btn_init(&b->detector, cfg);
}

bool button_is_pressed(const button_t *b)
{
    return hal_gpio_read(b->pin) == b->active_high;
}

btn_event_t button_update(button_t *b, uint32_t now_ms)
{
    return btn_update(&b->detector, button_is_pressed(b), now_ms);
}

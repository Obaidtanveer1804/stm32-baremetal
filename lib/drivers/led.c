/**
 * @file    led.c
 * @brief   LED patterns (see led.h).
 */
#include "led.h"
#include "hal_time.h"

/* heartbeat: on 60, off 140, on 60, off 740 ms */
static const uint16_t heartbeat_steps[4] = { 60U, 140U, 60U, 740U };

static void drive(led_t *led, bool on)
{
    led->on = on;
    hal_gpio_write(led->pin, on == led->active_high);
}

void led_init(led_t *led, hal_pin_t pin, bool active_high)
{
    led->pin         = pin;
    led->active_high = active_high;
    led->mode        = LED_MODE_STATIC;
    hal_gpio_mode(pin, HAL_GPIO_OUTPUT);
    drive(led, false);
}

void led_write(led_t *led, bool on)
{
    led->mode = LED_MODE_STATIC;
    drive(led, on);
}

void led_toggle(led_t *led)
{
    led_write(led, !led->on);
}

bool led_is_on(const led_t *led)
{
    return led->on;
}

void led_blink(led_t *led, uint16_t on_ms, uint16_t off_ms)
{
    led->mode    = LED_MODE_BLINK;
    led->on_ms   = on_ms;
    led->off_ms  = off_ms;
    led->next_ms = hal_time_ms();
    drive(led, false);
}

void led_flash(led_t *led, uint16_t count, uint16_t on_ms, uint16_t off_ms)
{
    led_blink(led, on_ms, off_ms);
    led->mode         = LED_MODE_FLASH;
    led->flashes_left = count;
}

void led_heartbeat(led_t *led)
{
    led->mode    = LED_MODE_HEARTBEAT;
    led->phase   = 0U;
    led->next_ms = hal_time_ms();
    drive(led, false);
}

void led_update(led_t *led, uint32_t now_ms)
{
    if ((led->mode == LED_MODE_STATIC) || ((int32_t)(now_ms - led->next_ms) < 0)) {
        return;
    }
    switch (led->mode) {
    case LED_MODE_HEARTBEAT:
        drive(led, (led->phase & 1U) == 0U);       /* even steps are "on" */
        led->next_ms = now_ms + heartbeat_steps[led->phase];
        led->phase   = (uint8_t)((led->phase + 1U) & 3U);
        break;
    case LED_MODE_FLASH:
        if (!led->on && (led->flashes_left == 0U)) {
            led->mode = LED_MODE_STATIC;
            break;
        }
        if (!led->on) {
            led->flashes_left--;
        }
        drive(led, !led->on);
        led->next_ms = now_ms + (led->on ? led->on_ms : led->off_ms);
        break;
    case LED_MODE_BLINK:
    default:
        drive(led, !led->on);
        led->next_ms = now_ms + (led->on ? led->on_ms : led->off_ms);
        break;
    }
}

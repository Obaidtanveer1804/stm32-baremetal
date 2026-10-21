/**
 * @file    led.h
 * @brief   LED objects with polarity and non-blocking patterns
 *          (blink, heartbeat, n flashes) driven from led_update().
 *
 *   led_t green;
 *   led_init(&green, BOARD_LED_GREEN, true);
 *   led_blink(&green, 100, 900);       // 100 ms on, 900 ms off
 *   for (;;) { led_update(&green, hal_time_ms()); ... }
 */
#ifndef LED_H
#define LED_H

#include "hal_gpio.h"

typedef enum {
    LED_MODE_STATIC = 0,
    LED_MODE_BLINK,
    LED_MODE_FLASH,       /**< n flashes then off */
    LED_MODE_HEARTBEAT    /**< double pulse, like a pulse monitor */
} led_mode_t;

typedef struct {
    hal_pin_t  pin;
    bool       active_high;
    bool       on;
    led_mode_t mode;
    uint16_t   on_ms;
    uint16_t   off_ms;
    uint16_t   flashes_left;
    uint8_t    phase;
    uint32_t   next_ms;
} led_t;

void led_init(led_t *led, hal_pin_t pin, bool active_high);
void led_write(led_t *led, bool on);        /**< stops any pattern */
void led_toggle(led_t *led);
bool led_is_on(const led_t *led);
void led_blink(led_t *led, uint16_t on_ms, uint16_t off_ms);
void led_flash(led_t *led, uint16_t count, uint16_t on_ms, uint16_t off_ms);
void led_heartbeat(led_t *led);
/** Call regularly (every few ms) with the current time. */
void led_update(led_t *led, uint32_t now_ms);

#endif /* LED_H */

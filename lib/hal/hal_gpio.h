/**
 * @file    hal_gpio.h
 * @brief   GPIO: pins encoded in one byte, single-pin and port-wide
 *          (masked) operations.
 *
 * A pin is (port << 4) | number, e.g. PC8 = 0x28. Port-wide functions
 * take a 16-bit mask so several pins change in one register write,
 * like gpio_set(GPIOC, GPIO8 | GPIO9) in libopencm3:
 *
 *   hal_gpio_port_write(HAL_PORT_C, BIT(8), BIT(9));  // PC8=1, PC9=0 atomically
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_types.h"

typedef enum {
    HAL_PORT_A = 0,
    HAL_PORT_B,
    HAL_PORT_C,
    HAL_PORT_D,
    HAL_PORT_E,
    HAL_PORT_COUNT
} hal_port_t;

typedef uint8_t hal_pin_t;

#define HAL_PIN(port, num)   ((hal_pin_t)((((uint32_t)(port)) << 4) | ((uint32_t)(num) & 0x0FU)))
#define HAL_PIN_PORT(pin)    ((hal_port_t)(((uint32_t)(pin)) >> 4))
#define HAL_PIN_NUM(pin)     ((uint32_t)(pin) & 0x0FU)
#define HAL_PIN_MASK(pin)    ((uint16_t)(1U << HAL_PIN_NUM(pin)))
#define HAL_PIN_NONE         ((hal_pin_t)0xFFU)

/* Short names: PA(5), PC(8) ... */
#define PA(n)  HAL_PIN(HAL_PORT_A, (n))
#define PB(n)  HAL_PIN(HAL_PORT_B, (n))
#define PC(n)  HAL_PIN(HAL_PORT_C, (n))
#define PD(n)  HAL_PIN(HAL_PORT_D, (n))
#define PE(n)  HAL_PIN(HAL_PORT_E, (n))

typedef enum {
    HAL_GPIO_INPUT = 0,         /**< floating input                      */
    HAL_GPIO_INPUT_PULLUP,
    HAL_GPIO_INPUT_PULLDOWN,
    HAL_GPIO_ANALOG,            /**< for ADC / DAC                       */
    HAL_GPIO_OUTPUT,            /**< push-pull, 2 MHz                    */
    HAL_GPIO_OUTPUT_FAST,       /**< push-pull, 50 MHz                   */
    HAL_GPIO_OUTPUT_OD,         /**< open drain                          */
    HAL_GPIO_AF,                /**< alternate function push-pull, 50 MHz */
    HAL_GPIO_AF_OD,             /**< alternate function open drain (I2C) */
    HAL_GPIO_MODE_COUNT
} hal_gpio_mode_t;

/** Configure one pin (also enables the port clock). */
void     hal_gpio_mode(hal_pin_t pin, hal_gpio_mode_t mode);
/** Configure every pin of @p mask on @p port at once. */
void     hal_gpio_port_mode(hal_port_t port, uint16_t mask, hal_gpio_mode_t mode);
hal_gpio_mode_t hal_gpio_get_mode(hal_pin_t pin);

void     hal_gpio_write(hal_pin_t pin, bool level);
void     hal_gpio_set(hal_pin_t pin);
void     hal_gpio_clear(hal_pin_t pin);
void     hal_gpio_toggle(hal_pin_t pin);
/** Input level (IDR) of a pin. */
bool     hal_gpio_read(hal_pin_t pin);
/** Output latch (ODR) of a pin: what we drive, not what we read. */
bool     hal_gpio_read_output(hal_pin_t pin);

/** Atomic set/reset through BSRR: bits in set_mask -> 1, clr_mask -> 0. */
void     hal_gpio_port_write(hal_port_t port, uint16_t set_mask, uint16_t clr_mask);
void     hal_gpio_port_toggle(hal_port_t port, uint16_t mask);
uint16_t hal_gpio_port_read(hal_port_t port);
uint16_t hal_gpio_port_read_output(hal_port_t port);

/* --------- portable helpers (hal_gpio_common.c, same on every port) */

/** Parse "PA5", "pc13", "C8" into a pin. */
bool        hal_gpio_parse(const char *name, hal_pin_t *pin);
/** "PC8" into @p buf (at least 5 bytes). Returns buf. */
const char *hal_gpio_name(hal_pin_t pin, char *buf);
const char *hal_gpio_mode_name(hal_gpio_mode_t mode);
bool        hal_gpio_parse_mode(const char *name, hal_gpio_mode_t *mode);
bool        hal_gpio_pin_valid(hal_pin_t pin);

#endif /* HAL_GPIO_H */

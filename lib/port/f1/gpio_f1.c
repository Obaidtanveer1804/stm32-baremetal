/**
 * @file    gpio_f1.c
 * @brief   hal_gpio on the STM32F1 GPIO block (CRL/CRH, ODR, IDR, BSRR).
 */
#include "hal_gpio.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

static const uint32_t port_base[HAL_PORT_COUNT] = {
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE
};

static const enum rcc_periph_clken port_clock[HAL_PORT_COUNT] = {
    RCC_GPIOA, RCC_GPIOB, RCC_GPIOC, RCC_GPIOD, RCC_GPIOE
};

/* Remembered per pin so the CLI can show it (the F1 encodes pull-up vs
 * pull-down in ODR, which makes reading the mode back ambiguous). */
static uint8_t  s_mode[HAL_PORT_COUNT][16];
static uint8_t  s_clock_on;     /* bit per port */

static void port_clock_enable(hal_port_t port)
{
    if ((s_clock_on & (1U << port)) == 0U) {
        rcc_periph_clock_enable(port_clock[port]);
        s_clock_on |= (uint8_t)(1U << port);
    }
}

void hal_gpio_port_mode(hal_port_t port, uint16_t mask, hal_gpio_mode_t mode)
{
    uint8_t  f1_mode;
    uint8_t  f1_cnf;
    uint32_t base;

    if ((port >= HAL_PORT_COUNT) || (mask == 0U)) {
        return;
    }
    base = port_base[port];
    port_clock_enable(port);

    switch (mode) {
    case HAL_GPIO_INPUT:
        f1_mode = GPIO_MODE_INPUT;         f1_cnf = GPIO_CNF_INPUT_FLOAT;         break;
    case HAL_GPIO_INPUT_PULLUP:
    case HAL_GPIO_INPUT_PULLDOWN:
        f1_mode = GPIO_MODE_INPUT;         f1_cnf = GPIO_CNF_INPUT_PULL_UPDOWN;   break;
    case HAL_GPIO_ANALOG:
        f1_mode = GPIO_MODE_INPUT;         f1_cnf = GPIO_CNF_INPUT_ANALOG;        break;
    case HAL_GPIO_OUTPUT_FAST:
        f1_mode = GPIO_MODE_OUTPUT_50_MHZ; f1_cnf = GPIO_CNF_OUTPUT_PUSHPULL;     break;
    case HAL_GPIO_OUTPUT_OD:
        f1_mode = GPIO_MODE_OUTPUT_2_MHZ;  f1_cnf = GPIO_CNF_OUTPUT_OPENDRAIN;    break;
    case HAL_GPIO_AF:
        f1_mode = GPIO_MODE_OUTPUT_50_MHZ; f1_cnf = GPIO_CNF_OUTPUT_ALTFN_PUSHPULL;  break;
    case HAL_GPIO_AF_OD:
        f1_mode = GPIO_MODE_OUTPUT_50_MHZ; f1_cnf = GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN; break;
    case HAL_GPIO_OUTPUT:
    default:
        f1_mode = GPIO_MODE_OUTPUT_2_MHZ;  f1_cnf = GPIO_CNF_OUTPUT_PUSHPULL;     break;
    }

    /* On the F1 the pull direction is selected by the ODR bit */
    if (mode == HAL_GPIO_INPUT_PULLUP) {
        gpio_set(base, mask);
    } else if (mode == HAL_GPIO_INPUT_PULLDOWN) {
        gpio_clear(base, mask);
    } else {
        /* output level unchanged */
    }
    gpio_set_mode(base, f1_mode, f1_cnf, mask);

    for (uint32_t pin = 0U; pin < 16U; pin++) {
        if ((mask & (1U << pin)) != 0U) {
            s_mode[port][pin] = (uint8_t)mode;
        }
    }
}

void hal_gpio_mode(hal_pin_t pin, hal_gpio_mode_t mode)
{
    hal_gpio_port_mode(HAL_PIN_PORT(pin), HAL_PIN_MASK(pin), mode);
}

hal_gpio_mode_t hal_gpio_get_mode(hal_pin_t pin)
{
    if (!hal_gpio_pin_valid(pin)) {
        return HAL_GPIO_INPUT;
    }
    return (hal_gpio_mode_t)s_mode[HAL_PIN_PORT(pin)][HAL_PIN_NUM(pin)];
}

void hal_gpio_write(hal_pin_t pin, bool level)
{
    if (level) {
        hal_gpio_set(pin);
    } else {
        hal_gpio_clear(pin);
    }
}

void hal_gpio_set(hal_pin_t pin)
{
    if (hal_gpio_pin_valid(pin)) {
        GPIO_BSRR(port_base[HAL_PIN_PORT(pin)]) = HAL_PIN_MASK(pin);
    }
}

void hal_gpio_clear(hal_pin_t pin)
{
    if (hal_gpio_pin_valid(pin)) {
        /* upper half of BSRR = reset bits */
        GPIO_BSRR(port_base[HAL_PIN_PORT(pin)]) = (uint32_t)HAL_PIN_MASK(pin) << 16;
    }
}

void hal_gpio_toggle(hal_pin_t pin)
{
    if (hal_gpio_pin_valid(pin)) {
        hal_gpio_port_toggle(HAL_PIN_PORT(pin), HAL_PIN_MASK(pin));
    }
}

bool hal_gpio_read(hal_pin_t pin)
{
    if (!hal_gpio_pin_valid(pin)) {
        return false;
    }
    return (GPIO_IDR(port_base[HAL_PIN_PORT(pin)]) & HAL_PIN_MASK(pin)) != 0U;
}

bool hal_gpio_read_output(hal_pin_t pin)
{
    if (!hal_gpio_pin_valid(pin)) {
        return false;
    }
    return (GPIO_ODR(port_base[HAL_PIN_PORT(pin)]) & HAL_PIN_MASK(pin)) != 0U;
}

void hal_gpio_port_write(hal_port_t port, uint16_t set_mask, uint16_t clr_mask)
{
    if (port < HAL_PORT_COUNT) {
        /* one store: no read-modify-write, safe against interrupts.
         * If a bit is in both masks, set wins (hardware rule). */
        GPIO_BSRR(port_base[port]) = ((uint32_t)clr_mask << 16) | set_mask;
    }
}

void hal_gpio_port_toggle(hal_port_t port, uint16_t mask)
{
    if (port < HAL_PORT_COUNT) {
        /* toggle = set the bits now 0, reset the bits now 1, one BSRR write */
        const uint32_t odr = GPIO_ODR(port_base[port]);
        GPIO_BSRR(port_base[port]) = ((odr & mask) << 16) | (~odr & mask);
    }
}

uint16_t hal_gpio_port_read(hal_port_t port)
{
    return (port < HAL_PORT_COUNT) ? (uint16_t)GPIO_IDR(port_base[port]) : 0U;
}

uint16_t hal_gpio_port_read_output(hal_port_t port)
{
    return (port < HAL_PORT_COUNT) ? (uint16_t)GPIO_ODR(port_base[port]) : 0U;
}

/**
 * @file    opencm3_gpio_pc.c
 * @brief   libopencm3 GPIO calls (gpio_set, gpio_toggle...) mapped onto
 *          the ELISE hal_gpio, so on the PC they reach the simulated
 *          ports: "[sim] PC1 -> 1" on every change, readable with
 *          hal_gpio_* and the CLI's peek on GPIOC_ODR.
 */
#include <libopencm3/stm32/gpio.h>
#include "hal_gpio.h"

#define GPIO_PORT_STRIDE   (0x400U)
#define GPIO_PORT_COUNT    (5U)          /* A..E */
#define GPIO_PINS_PER_PORT (16U)

/* GPIOC (0x40011000) -> HAL_PORT_C. Unknown address: false. */
static bool port_from_address(uint32_t gpioport, hal_port_t *port)
{
    if ((gpioport < GPIO_PORT_A_BASE) ||
        (((gpioport - GPIO_PORT_A_BASE) % GPIO_PORT_STRIDE) != 0U)) {
        return false;
    }
    const uint32_t index = (gpioport - GPIO_PORT_A_BASE) / GPIO_PORT_STRIDE;
    if (index >= GPIO_PORT_COUNT) {
        return false;
    }
    *port = (hal_port_t)index;
    return true;
}

/* MODE + CNF fields of CRL/CRH -> the equivalent hal mode. */
static hal_gpio_mode_t mode_from_fields(uint8_t mode, uint8_t cnf)
{
    if (mode == GPIO_MODE_INPUT) {
        switch (cnf) {
        case GPIO_CNF_INPUT_ANALOG:      return HAL_GPIO_ANALOG;
        case GPIO_CNF_INPUT_PULL_UPDOWN: return HAL_GPIO_INPUT_PULLDOWN; /* ODR picks up/down on chip */
        default:                         return HAL_GPIO_INPUT;
        }
    }
    switch (cnf) {
    case GPIO_CNF_OUTPUT_OPENDRAIN:       return HAL_GPIO_OUTPUT_OD;
    case GPIO_CNF_OUTPUT_ALTFN_PUSHPULL:  return HAL_GPIO_AF;
    case GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN: return HAL_GPIO_AF_OD;
    default:
        return (mode == GPIO_MODE_OUTPUT_50_MHZ) ? HAL_GPIO_OUTPUT_FAST : HAL_GPIO_OUTPUT;
    }
}

void gpio_set_mode(uint32_t gpioport, uint8_t mode, uint8_t cnf, uint16_t gpios)
{
    hal_port_t port;
    if (port_from_address(gpioport, &port)) {
        hal_gpio_port_mode(port, gpios, mode_from_fields(mode, cnf));
    }
}

void gpio_set(uint32_t gpioport, uint16_t gpios)
{
    hal_port_t port;
    if (port_from_address(gpioport, &port)) {
        hal_gpio_port_write(port, gpios, 0U);
    }
}

void gpio_clear(uint32_t gpioport, uint16_t gpios)
{
    hal_port_t port;
    if (port_from_address(gpioport, &port)) {
        hal_gpio_port_write(port, 0U, gpios);
    }
}

void gpio_toggle(uint32_t gpioport, uint16_t gpios)
{
    hal_port_t port;
    if (port_from_address(gpioport, &port)) {
        hal_gpio_port_toggle(port, gpios);
    }
}

uint16_t gpio_get(uint32_t gpioport, uint16_t gpios)
{
    return (uint16_t)(gpio_port_read(gpioport) & gpios);
}

uint16_t gpio_port_read(uint32_t gpioport)
{
    hal_port_t port;
    return port_from_address(gpioport, &port) ? hal_gpio_port_read(port) : 0U;
}

void gpio_port_write(uint32_t gpioport, uint16_t data)
{
    hal_port_t port;
    if (port_from_address(gpioport, &port)) {
        hal_gpio_port_write(port, data, (uint16_t)~data);
    }
}

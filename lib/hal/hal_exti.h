/**
 * @file    hal_exti.h
 * @brief   External interrupts on GPIO edges, with per-line callbacks.
 *
 * The F1 has 16 EXTI lines: line n can watch pin n of ONE port only
 * (PA0 and PB0 cannot both interrupt). hal_exti_attach() returns
 * HAL_EBUSY in that case.
 *
 * On the PC port, sim_gpio_input() (CLI "sim pin PA0 1") generates
 * the edges, so interrupt-driven code is testable without a board.
 */
#ifndef HAL_EXTI_H
#define HAL_EXTI_H

#include "hal_types.h"
#include "hal_gpio.h"

typedef enum {
    HAL_EXTI_RISING  = (1U << 0),
    HAL_EXTI_FALLING = (1U << 1),
    HAL_EXTI_BOTH    = HAL_EXTI_RISING | HAL_EXTI_FALLING
} hal_exti_edge_t;

typedef void (*hal_exti_cb_t)(hal_pin_t pin, void *arg);

hal_status_t hal_exti_attach(hal_pin_t pin, hal_exti_edge_t edge,
                             hal_exti_cb_t cb, void *arg);
void         hal_exti_detach(hal_pin_t pin);
void         hal_exti_enable(hal_pin_t pin, bool enable);
/** Interrupt count per line since attach (for the CLI). */
uint32_t     hal_exti_count(hal_pin_t pin);

#endif /* HAL_EXTI_H */

/**
 * @file    hal_types.h
 * @brief   Types shared by every HAL module.
 *
 * The HAL ("hardware abstraction layer") is the only layer that knows
 * about registers. Everything above it (core/, drivers/, cli/, apps/)
 * only calls hal_*() functions, so it can be linked against:
 *
 *   port/f1/  -> real STM32F1 peripherals through libopencm3
 *   port/pc/  -> a simulation that prints / models the hardware
 *
 * Same idea as uart_f1.c / uart_pc.c in the TP, applied to every
 * peripheral.
 */
#ifndef HAL_TYPES_H
#define HAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR       = -1,   /**< generic failure                        */
    HAL_EINVAL      = -2,   /**< bad argument                           */
    HAL_ETIMEOUT    = -3,   /**< peripheral did not answer in time      */
    HAL_ENACK       = -4,   /**< I2C: no acknowledge                    */
    HAL_EBUSY       = -5,
    HAL_EUNSUPPORTED = -6   /**< not available on this chip / port      */
} hal_status_t;

/** Human readable status (for the CLI). */
const char *hal_status_str(hal_status_t st);

#endif /* HAL_TYPES_H */

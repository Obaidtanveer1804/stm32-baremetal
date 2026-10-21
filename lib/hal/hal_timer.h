/**
 * @file    hal_timer.h
 * @brief   Periodic timer interrupts and tick counters on TIM2 / TIM3 /
 *          TIM4.
 *
 * The prescaler (PSC) and auto-reload (ARR) are computed for you from
 * a frequency: f_update = f_timer / ((PSC + 1) * (ARR + 1)).
 *
 * TIM3 and TIM4 are also used by hal_pwm: don't start a periodic
 * interrupt and PWM on the same timer with different frequencies.
 */
#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "hal_types.h"

typedef enum {
    HAL_TIMER2 = 0,
    HAL_TIMER3,
    HAL_TIMER4,
    HAL_TIMER_COUNT
} hal_timer_t;

typedef void (*hal_timer_cb_t)(hal_timer_t timer, void *arg);

/**
 * @brief  Call @p cb at @p freq_hz from the timer update interrupt.
 * @return HAL_EINVAL if the frequency cannot be reached.
 */
hal_status_t hal_timer_start_periodic(hal_timer_t timer, uint32_t freq_hz,
                                      hal_timer_cb_t cb, void *arg);
void         hal_timer_stop(hal_timer_t timer);
bool         hal_timer_is_running(hal_timer_t timer);
/** Change the frequency of a running timer (glitch-free, ARR preload). */
hal_status_t hal_timer_set_frequency(hal_timer_t timer, uint32_t freq_hz);
/** Frequency actually obtained (integer rounding of PSC/ARR). */
uint32_t     hal_timer_get_frequency(hal_timer_t timer);
/** Number of update interrupts since start. */
uint32_t     hal_timer_get_ticks(hal_timer_t timer);
/** Raw CNT register. */
uint16_t     hal_timer_get_counter(hal_timer_t timer);

/** Timer kernel clock in Hz (x2 when the APB prescaler is not 1). */
uint32_t     hal_timer_clock_hz(hal_timer_t timer);

/**
 * @brief  Compute PSC and ARR for a frequency (portable, unit-tested).
 * @return false if out of range.
 */
bool hal_timer_calc(uint32_t clk_hz, uint32_t freq_hz, uint16_t *psc, uint16_t *arr);

#endif /* HAL_TIMER_H */

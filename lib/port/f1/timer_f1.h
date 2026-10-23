/**
 * @file    timer_f1.h
 * @brief   Private to the F1 port: timer base shared by hal_timer and
 *          hal_pwm (both drive TIM3/TIM4).
 */
#ifndef TIMER_F1_H
#define TIMER_F1_H

#include "hal_timer.h"

/** libopencm3 base address of a HAL timer (TIM2, TIM3, TIM4). */
uint32_t     f1_timer_base(hal_timer_t timer);
/** Clock, reset, PSC/ARR, up-counting, preload; counter enabled. */
hal_status_t f1_timer_setup_base(hal_timer_t timer, uint32_t freq_hz);
/** Current ARR value (period - 1). */
uint16_t     f1_timer_arr(hal_timer_t timer);

#endif /* TIMER_F1_H */

/**
 * @file    hal_pwm.h
 * @brief   PWM outputs on TIM3 and TIM4 (8 channels).
 *
 *   channel        default pin   with BOARD_TIM3_FULL_REMAP (VL-Discovery)
 *   HAL_PWM_T3C1   PA6           PC6
 *   HAL_PWM_T3C2   PA7           PC7
 *   HAL_PWM_T3C3   PB0           PC8  (blue LED)
 *   HAL_PWM_T3C4   PB1           PC9  (green LED)
 *   HAL_PWM_T4C1   PB6
 *   HAL_PWM_T4C2   PB7
 *   HAL_PWM_T4C3   PB8
 *   HAL_PWM_T4C4   PB9
 *
 * The four channels of one timer share the same frequency.
 * Duty is given in per-mille (0..1000) so it does not depend on ARR.
 */
#ifndef HAL_PWM_H
#define HAL_PWM_H

#include "hal_types.h"
#include "hal_gpio.h"

typedef enum {
    HAL_PWM_T3C1 = 0,
    HAL_PWM_T3C2,
    HAL_PWM_T3C3,
    HAL_PWM_T3C4,
    HAL_PWM_T4C1,
    HAL_PWM_T4C2,
    HAL_PWM_T4C3,
    HAL_PWM_T4C4,
    HAL_PWM_COUNT
} hal_pwm_t;

#define HAL_PWM_DUTY_MAX  (1000U)

hal_status_t hal_pwm_init(hal_pwm_t ch, uint32_t freq_hz);
void         hal_pwm_set_duty(hal_pwm_t ch, uint32_t permille);
uint32_t     hal_pwm_get_duty(hal_pwm_t ch);
/** Pulse width in timer ticks (fine control, e.g. servo pulses). */
void         hal_pwm_set_pulse_ticks(hal_pwm_t ch, uint16_t ticks);
/** Servo helper: pulse width in microseconds (needs 50 Hz). */
void         hal_pwm_set_pulse_us(hal_pwm_t ch, uint32_t us);
void         hal_pwm_stop(hal_pwm_t ch);
hal_pin_t    hal_pwm_pin(hal_pwm_t ch);
uint32_t     hal_pwm_get_frequency(hal_pwm_t ch);

#endif /* HAL_PWM_H */

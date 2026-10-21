/**
 * @file    pwm_f1.c
 * @brief   hal_pwm: PWM mode 1 on TIM3 / TIM4 output-compare channels.
 */
#include "hal_pwm.h"
#include "hal_gpio.h"
#include "timer_f1.h"
#include "board.h"
#include "f1_common.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

typedef struct {
    hal_timer_t     timer;
    enum tim_oc_id  oc;
    hal_pin_t       pin;
} pwm_hw_t;

static const pwm_hw_t pwm_hw[HAL_PWM_COUNT] = {
#if BOARD_TIM3_FULL_REMAP
    { HAL_TIMER3, TIM_OC1, PC(6) },
    { HAL_TIMER3, TIM_OC2, PC(7) },
    { HAL_TIMER3, TIM_OC3, PC(8) },
    { HAL_TIMER3, TIM_OC4, PC(9) },
#else
    { HAL_TIMER3, TIM_OC1, PA(6) },
    { HAL_TIMER3, TIM_OC2, PA(7) },
    { HAL_TIMER3, TIM_OC3, PB(0) },
    { HAL_TIMER3, TIM_OC4, PB(1) },
#endif
    { HAL_TIMER4, TIM_OC1, PB(6) },
    { HAL_TIMER4, TIM_OC2, PB(7) },
    { HAL_TIMER4, TIM_OC3, PB(8) },
    { HAL_TIMER4, TIM_OC4, PB(9) },
};

static uint16_t s_duty_permille[HAL_PWM_COUNT];

hal_status_t hal_pwm_init(hal_pwm_t ch, uint32_t freq_hz)
{
    if (ch >= HAL_PWM_COUNT) {
        return HAL_EINVAL;
    }
    const pwm_hw_t *hw = &pwm_hw[ch];

#if BOARD_TIM3_FULL_REMAP
    if (hw->timer == HAL_TIMER3) {
        rcc_periph_clock_enable(RCC_AFIO);
        gpio_primary_remap(F1_SWJ_CONFIG, AFIO_MAPR_TIM3_REMAP_FULL_REMAP);
    }
#endif
    const hal_status_t st = f1_timer_setup_base(hw->timer, freq_hz);
    if (st != HAL_OK) {
        return st;
    }
    const uint32_t base = f1_timer_base(hw->timer);
    timer_set_oc_mode(base, hw->oc, TIM_OCM_PWM1);
    timer_enable_oc_preload(base, hw->oc);
    timer_set_oc_value(base, hw->oc, 0U);
    timer_enable_oc_output(base, hw->oc);
    hal_gpio_mode(hw->pin, HAL_GPIO_AF);
    s_duty_permille[ch] = 0U;
    return HAL_OK;
}

void hal_pwm_set_pulse_ticks(hal_pwm_t ch, uint16_t ticks)
{
    if (ch < HAL_PWM_COUNT) {
        timer_set_oc_value(f1_timer_base(pwm_hw[ch].timer), pwm_hw[ch].oc, ticks);
    }
}

void hal_pwm_set_duty(hal_pwm_t ch, uint32_t permille)
{
    if (ch >= HAL_PWM_COUNT) {
        return;
    }
    if (permille > HAL_PWM_DUTY_MAX) {
        permille = HAL_PWM_DUTY_MAX;
    }
    /* CCR = duty * (ARR + 1): CCR > ARR gives a 100 % output */
    const uint32_t period = (uint32_t)f1_timer_arr(pwm_hw[ch].timer) + 1U;
    const uint32_t ccr    = ((period * permille) + (HAL_PWM_DUTY_MAX / 2U)) / HAL_PWM_DUTY_MAX;
    timer_set_oc_value(f1_timer_base(pwm_hw[ch].timer), pwm_hw[ch].oc, ccr);
    s_duty_permille[ch] = (uint16_t)permille;
}

uint32_t hal_pwm_get_duty(hal_pwm_t ch)
{
    return (ch < HAL_PWM_COUNT) ? s_duty_permille[ch] : 0U;
}

void hal_pwm_set_pulse_us(hal_pwm_t ch, uint32_t us)
{
    if (ch >= HAL_PWM_COUNT) {
        return;
    }
    const uint32_t freq   = hal_timer_get_frequency(pwm_hw[ch].timer);
    const uint32_t period = (uint32_t)f1_timer_arr(pwm_hw[ch].timer) + 1U;
    if (freq == 0U) {
        return;
    }
    /* ticks = us * period * freq / 1e6, in 64 bit to avoid overflow */
    const uint64_t ticks = ((uint64_t)us * period * freq) / 1000000ULL;
    hal_pwm_set_pulse_ticks(ch, (uint16_t)((ticks > 0xFFFFU) ? 0xFFFFU : ticks));
}

void hal_pwm_stop(hal_pwm_t ch)
{
    if (ch < HAL_PWM_COUNT) {
        timer_disable_oc_output(f1_timer_base(pwm_hw[ch].timer), pwm_hw[ch].oc);
        hal_gpio_mode(pwm_hw[ch].pin, HAL_GPIO_OUTPUT);
        hal_gpio_clear(pwm_hw[ch].pin);
        s_duty_permille[ch] = 0U;
    }
}

hal_pin_t hal_pwm_pin(hal_pwm_t ch)
{
    return (ch < HAL_PWM_COUNT) ? pwm_hw[ch].pin : HAL_PIN_NONE;
}

uint32_t hal_pwm_get_frequency(hal_pwm_t ch)
{
    return (ch < HAL_PWM_COUNT) ? hal_timer_get_frequency(pwm_hw[ch].timer) : 0U;
}

/**
 * @file    timer_f1.c
 * @brief   hal_timer on TIM2..TIM4 (general purpose timers, APB1).
 */
#include "hal_timer.h"
#include "timer_f1.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

typedef struct {
    uint32_t              base;
    enum rcc_periph_clken clock;
    enum rcc_periph_rst   reset;
    uint8_t               irq;
} timer_hw_t;

static const timer_hw_t timer_hw[HAL_TIMER_COUNT] = {
    { TIM2, RCC_TIM2, RST_TIM2, NVIC_TIM2_IRQ },
    { TIM3, RCC_TIM3, RST_TIM3, NVIC_TIM3_IRQ },
    { TIM4, RCC_TIM4, RST_TIM4, NVIC_TIM4_IRQ },
};

typedef struct {
    hal_timer_cb_t    cb;
    void             *arg;
    volatile uint32_t ticks;
    uint32_t          freq_hz;
    uint16_t          psc;
    uint16_t          arr;
    bool              running;
} timer_state_t;

static timer_state_t s_timer[HAL_TIMER_COUNT];

uint32_t f1_timer_base(hal_timer_t timer)
{
    return timer_hw[timer].base;
}

uint16_t f1_timer_arr(hal_timer_t timer)
{
    return s_timer[timer].arr;
}

uint32_t hal_timer_clock_hz(hal_timer_t timer)
{
    (void)timer;   /* TIM2-4 are all on APB1 */
    /* RM0008 7.2: timer clock = APB1 x1 if APB1 prescaler is 1, else x2 */
    return (rcc_apb1_frequency == rcc_ahb_frequency) ? rcc_apb1_frequency
                                                     : (2U * rcc_apb1_frequency);
}

hal_status_t f1_timer_setup_base(hal_timer_t timer, uint32_t freq_hz)
{
    uint16_t psc;
    uint16_t arr;

    if (timer >= HAL_TIMER_COUNT) {
        return HAL_EINVAL;
    }
    if (!hal_timer_calc(hal_timer_clock_hz(timer), freq_hz, &psc, &arr)) {
        return HAL_EINVAL;
    }
    const timer_hw_t *hw = &timer_hw[timer];

    rcc_periph_clock_enable(hw->clock);
    if (!s_timer[timer].running) {
        rcc_periph_reset_pulse(hw->reset);
    }
    timer_set_mode(hw->base, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(hw->base, psc);
    timer_set_period(hw->base, arr);
    timer_enable_preload(hw->base);
    /* UG event loads PSC immediately (otherwise only at next overflow);
     * URS keeps that event from raising an update interrupt */
    timer_update_on_overflow(hw->base);
    timer_generate_event(hw->base, TIM_EGR_UG);
    timer_enable_counter(hw->base);

    s_timer[timer].psc     = psc;
    s_timer[timer].arr     = arr;
    s_timer[timer].freq_hz = hal_timer_clock_hz(timer) / (((uint32_t)psc + 1U) * ((uint32_t)arr + 1U));
    s_timer[timer].running = true;
    return HAL_OK;
}

hal_status_t hal_timer_start_periodic(hal_timer_t timer, uint32_t freq_hz,
                                      hal_timer_cb_t cb, void *arg)
{
    if (timer >= HAL_TIMER_COUNT) {
        return HAL_EINVAL;
    }
    s_timer[timer].cb    = cb;
    s_timer[timer].arg   = arg;
    s_timer[timer].ticks = 0U;

    const hal_status_t st = f1_timer_setup_base(timer, freq_hz);
    if (st != HAL_OK) {
        return st;
    }
    timer_clear_flag(timer_hw[timer].base, TIM_SR_UIF);
    timer_enable_irq(timer_hw[timer].base, TIM_DIER_UIE);
    nvic_enable_irq(timer_hw[timer].irq);
    return HAL_OK;
}

void hal_timer_stop(hal_timer_t timer)
{
    if (timer < HAL_TIMER_COUNT) {
        timer_disable_irq(timer_hw[timer].base, TIM_DIER_UIE);
        timer_disable_counter(timer_hw[timer].base);
        nvic_disable_irq(timer_hw[timer].irq);
        s_timer[timer].running = false;
        s_timer[timer].cb      = NULL;
    }
}

bool hal_timer_is_running(hal_timer_t timer)
{
    return (timer < HAL_TIMER_COUNT) && s_timer[timer].running;
}

hal_status_t hal_timer_set_frequency(hal_timer_t timer, uint32_t freq_hz)
{
    uint16_t psc;
    uint16_t arr;

    if ((timer >= HAL_TIMER_COUNT) ||
        !hal_timer_calc(hal_timer_clock_hz(timer), freq_hz, &psc, &arr)) {
        return HAL_EINVAL;
    }
    /* with preload enabled the new values apply at the next update:
     * no shortened or doubled period */
    timer_set_prescaler(timer_hw[timer].base, psc);
    timer_set_period(timer_hw[timer].base, arr);
    s_timer[timer].psc     = psc;
    s_timer[timer].arr     = arr;
    s_timer[timer].freq_hz = hal_timer_clock_hz(timer) / (((uint32_t)psc + 1U) * ((uint32_t)arr + 1U));
    return HAL_OK;
}

uint32_t hal_timer_get_frequency(hal_timer_t timer)
{
    return (timer < HAL_TIMER_COUNT) ? s_timer[timer].freq_hz : 0U;
}

uint32_t hal_timer_get_ticks(hal_timer_t timer)
{
    return (timer < HAL_TIMER_COUNT) ? s_timer[timer].ticks : 0U;
}

uint16_t hal_timer_get_counter(hal_timer_t timer)
{
    return (timer < HAL_TIMER_COUNT) ? (uint16_t)timer_get_counter(timer_hw[timer].base) : 0U;
}

/* ------------------------- interrupt handlers --------------------- */

static void timer_isr(hal_timer_t timer)
{
    const uint32_t base = timer_hw[timer].base;
    if (timer_get_flag(base, TIM_SR_UIF)) {
        timer_clear_flag(base, TIM_SR_UIF);
        s_timer[timer].ticks++;
        if (s_timer[timer].cb != NULL) {
            s_timer[timer].cb(timer, s_timer[timer].arg);
        }
    }
}

void tim2_isr(void) { timer_isr(HAL_TIMER2); }
void tim3_isr(void) { timer_isr(HAL_TIMER3); }
void tim4_isr(void) { timer_isr(HAL_TIMER4); }

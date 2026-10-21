/**
 * @file    pwm_pc.c
 * @brief   Simulated PWM: same channel/pin table and CCR arithmetic as
 *          pwm_f1.c; duty changes are logged and shown in "sim status".
 */
#include "sim_internal.h"
#include "board.h"

typedef struct {
    hal_timer_t timer;
    uint8_t     channel;    /* 1..4 */
    hal_pin_t   pin;
} pwm_map_t;

static const pwm_map_t pwm_map[HAL_PWM_COUNT] = {
#if BOARD_TIM3_FULL_REMAP
    { HAL_TIMER3, 1U, PC(6) }, { HAL_TIMER3, 2U, PC(7) },
    { HAL_TIMER3, 3U, PC(8) }, { HAL_TIMER3, 4U, PC(9) },
#else
    { HAL_TIMER3, 1U, PA(6) }, { HAL_TIMER3, 2U, PA(7) },
    { HAL_TIMER3, 3U, PB(0) }, { HAL_TIMER3, 4U, PB(1) },
#endif
    { HAL_TIMER4, 1U, PB(6) }, { HAL_TIMER4, 2U, PB(7) },
    { HAL_TIMER4, 3U, PB(8) }, { HAL_TIMER4, 4U, PB(9) },
};

static uint16_t s_ccr[HAL_PWM_COUNT];
static uint16_t s_permille[HAL_PWM_COUNT];
static bool     s_enabled[HAL_PWM_COUNT];

static void log_duty(hal_pwm_t ch)
{
    char name[6];
    const uint32_t period = (uint32_t)sim_timer_arr(pwm_map[ch].timer) + 1U;
    const uint32_t pm     = (s_ccr[ch] >= period) ? 1000U : ((s_ccr[ch] * 1000U) / period);
    sim_log(SIM_LOG_PWM, "T%uC%u (%s) duty %lu.%lu%% ccr=%u/%lu", (unsigned)pwm_map[ch].timer + 2U,
            (unsigned)pwm_map[ch].channel, hal_gpio_name(pwm_map[ch].pin, name),
            (unsigned long)(pm / 10U), (unsigned long)(pm % 10U), (unsigned)s_ccr[ch],
            (unsigned long)period);
}

hal_status_t hal_pwm_init(hal_pwm_t ch, uint32_t freq_hz)
{
    if (ch >= HAL_PWM_COUNT) {
        return HAL_EINVAL;
    }
    const hal_status_t st = sim_timer_setup_base(pwm_map[ch].timer, freq_hz);
    if (st != HAL_OK) {
        return st;
    }
    hal_gpio_mode(pwm_map[ch].pin, HAL_GPIO_AF);
    s_ccr[ch]      = 0U;
    s_permille[ch] = 0U;
    s_enabled[ch]  = true;
    return HAL_OK;
}

void hal_pwm_set_pulse_ticks(hal_pwm_t ch, uint16_t ticks)
{
    if ((ch < HAL_PWM_COUNT) && (s_ccr[ch] != ticks)) {
        s_ccr[ch] = ticks;
        log_duty(ch);
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
    const uint32_t period = (uint32_t)sim_timer_arr(pwm_map[ch].timer) + 1U;
    const uint32_t ccr    = ((period * permille) + (HAL_PWM_DUTY_MAX / 2U)) / HAL_PWM_DUTY_MAX;
    s_permille[ch] = (uint16_t)permille;
    hal_pwm_set_pulse_ticks(ch, (uint16_t)((ccr > 0xFFFFU) ? 0xFFFFU : ccr));
}

uint32_t hal_pwm_get_duty(hal_pwm_t ch)
{
    return (ch < HAL_PWM_COUNT) ? s_permille[ch] : 0U;
}

void hal_pwm_set_pulse_us(hal_pwm_t ch, uint32_t us)
{
    if (ch >= HAL_PWM_COUNT) {
        return;
    }
    const uint32_t freq   = hal_timer_get_frequency(pwm_map[ch].timer);
    const uint32_t period = (uint32_t)sim_timer_arr(pwm_map[ch].timer) + 1U;
    const uint64_t ticks  = ((uint64_t)us * period * freq) / 1000000ULL;
    hal_pwm_set_pulse_ticks(ch, (uint16_t)((ticks > 0xFFFFU) ? 0xFFFFU : ticks));
}

void hal_pwm_stop(hal_pwm_t ch)
{
    if (ch < HAL_PWM_COUNT) {
        s_enabled[ch]  = false;
        s_ccr[ch]      = 0U;
        s_permille[ch] = 0U;
        hal_gpio_mode(pwm_map[ch].pin, HAL_GPIO_OUTPUT);
        hal_gpio_clear(pwm_map[ch].pin);
    }
}

hal_pin_t hal_pwm_pin(hal_pwm_t ch)
{
    return (ch < HAL_PWM_COUNT) ? pwm_map[ch].pin : HAL_PIN_NONE;
}

uint32_t hal_pwm_get_frequency(hal_pwm_t ch)
{
    return (ch < HAL_PWM_COUNT) ? hal_timer_get_frequency(pwm_map[ch].timer) : 0U;
}

uint16_t sim_pwm_ccr(hal_timer_t timer, uint32_t channel)
{
    for (uint32_t ch = 0U; ch < HAL_PWM_COUNT; ch++) {
        if ((pwm_map[ch].timer == timer) && (pwm_map[ch].channel == channel)) {
            return s_ccr[ch];
        }
    }
    return 0U;
}

bool sim_pwm_enabled(hal_timer_t timer, uint32_t channel)
{
    for (uint32_t ch = 0U; ch < HAL_PWM_COUNT; ch++) {
        if ((pwm_map[ch].timer == timer) && (pwm_map[ch].channel == channel)) {
            return s_enabled[ch];
        }
    }
    return false;
}

void sim_pwm_status(void)
{
    for (uint32_t ch = 0U; ch < HAL_PWM_COUNT; ch++) {
        if (s_enabled[ch]) {
            char name[6];
            const uint32_t bar = (s_permille[ch] + 25U) / 50U;   /* 0..20 chars */
            sim_printf("PWM T%uC%u %-4s %4lu.%lu%% |", (unsigned)pwm_map[ch].timer + 2U,
                       (unsigned)pwm_map[ch].channel, hal_gpio_name(pwm_map[ch].pin, name),
                       (unsigned long)(s_permille[ch] / 10U), (unsigned long)(s_permille[ch] % 10U));
            for (uint32_t i = 0U; i < 20U; i++) {
                sim_printf("%c", (i < bar) ? '#' : ' ');
            }
            sim_printf("|\r\n");
        }
    }
}

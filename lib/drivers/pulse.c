/**
 * @file    pulse.c
 * @brief   EXTI-based pulse measurement and HC-SR04 (see pulse.h).
 */
#include "pulse.h"
#include "hal_exti.h"
#include "hal_time.h"

#define HCSR04_TRIG_US      (12U)       /* datasheet: >= 10 us     */
#define HCSR04_TIMEOUT_MS   (30U)       /* ~5 m round trip + margin */

/* runs in interrupt context on both edges */
static void edge_isr(hal_pin_t pin, void *arg)
{
    pulse_meter_t *p   = (pulse_meter_t *)arg;
    const uint32_t now = hal_time_us();

    if (hal_gpio_read(pin)) {
        if (p->edges > 0U) {
            p->period_us = now - p->last_rise_us;
            p->fresh     = true;
        }
        p->last_rise_us = now;
        p->edges++;
    } else {
        p->last_fall_us = now;
        if (p->edges > 0U) {
            p->high_us = now - p->last_rise_us;
        }
    }
}

hal_status_t pulse_meter_start(pulse_meter_t *p, hal_pin_t pin)
{
    p->pin       = pin;
    p->period_us = 0U;
    p->high_us   = 0U;
    p->edges     = 0U;
    p->fresh     = false;
    hal_gpio_mode(pin, HAL_GPIO_INPUT);
    return hal_exti_attach(pin, HAL_EXTI_BOTH, edge_isr, p);
}

void pulse_meter_stop(pulse_meter_t *p)
{
    hal_exti_detach(p->pin);
}

uint32_t pulse_meter_freq_c(const pulse_meter_t *p)
{
    const uint32_t period = p->period_us;
    return (period == 0U) ? 0U : (uint32_t)((100000000ULL + (period / 2U)) / period);
}

uint32_t pulse_meter_duty(const pulse_meter_t *p)
{
    const uint32_t period = p->period_us;
    return (period == 0U) ? 0U : (uint32_t)(((uint64_t)p->high_us * 1000U) / period);
}

uint32_t pulse_count_hz(pulse_meter_t *p, uint32_t gate_ms)
{
    const uint32_t e0 = p->edges;
    hal_delay_ms(gate_ms);
    const uint32_t n = p->edges - e0;
    return (gate_ms == 0U) ? 0U : ((n * 1000U) / gate_ms);
}

/* ----------------------------- HC-SR04 ---------------------------- */

uint32_t hcsr04_us_to_mm(uint32_t echo_us)
{
    /* sound goes there and back: d = t * v / 2, v = 0.343 mm/us */
    return (uint32_t)(((uint64_t)echo_us * 343U) / 2000U);
}

hal_status_t hcsr04_init(hcsr04_t *s, hal_pin_t trig, hal_pin_t echo)
{
    s->trig = trig;
    hal_gpio_mode(trig, HAL_GPIO_OUTPUT);
    hal_gpio_clear(trig);
    return pulse_meter_start(&s->echo, echo);
}

hal_status_t hcsr04_measure_mm(hcsr04_t *s, uint32_t *mm)
{
    const uint32_t falls_before = s->echo.last_fall_us;
    hal_gpio_set(s->trig);
    hal_delay_us(HCSR04_TRIG_US);
    hal_gpio_clear(s->trig);

    const uint32_t t0 = hal_time_ms();
    /* wait for the falling edge that ends the echo pulse */
    while (s->echo.last_fall_us == falls_before) {
        if ((hal_time_ms() - t0) > HCSR04_TIMEOUT_MS) {
            return HAL_ETIMEOUT;        /* nothing in range, or not wired */
        }
    }
    *mm = hcsr04_us_to_mm(s->echo.high_us);
    return HAL_OK;
}

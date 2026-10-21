/**
 * @file    pulse.h
 * @brief   Pulse and frequency measurement on any EXTI-capable pin,
 *          timestamped with hal_time_us() (1 us resolution):
 *            - period / frequency (rising to rising)
 *            - high time / duty cycle
 *            - edge counting over a gate time (frequency counter)
 *          Also the HC-SR04 ultrasonic range finder built on it.
 *
 * Good up to a few kHz (one interrupt per edge). Above that use a
 * timer in external clock / input capture mode.
 */
#ifndef PULSE_H
#define PULSE_H

#include "hal_gpio.h"
#include "hal_types.h"

typedef struct {
    hal_pin_t         pin;
    volatile uint32_t last_rise_us;
    volatile uint32_t last_fall_us;
    volatile uint32_t period_us;
    volatile uint32_t high_us;
    volatile uint32_t edges;          /**< rising edges since start */
    volatile bool     fresh;          /**< a new period was measured */
} pulse_meter_t;

hal_status_t pulse_meter_start(pulse_meter_t *p, hal_pin_t pin);
void         pulse_meter_stop(pulse_meter_t *p);
/** Frequency in centi-hertz from the last period (0 if none). */
uint32_t     pulse_meter_freq_c(const pulse_meter_t *p);
/** Duty cycle in per-mille from the last period. */
uint32_t     pulse_meter_duty(const pulse_meter_t *p);
/** Count rising edges for @p gate_ms (blocking): frequency counter. */
uint32_t     pulse_count_hz(pulse_meter_t *p, uint32_t gate_ms);

/* ----------------------------- HC-SR04 ---------------------------- */
typedef struct {
    hal_pin_t     trig;
    pulse_meter_t echo;
} hcsr04_t;

hal_status_t hcsr04_init(hcsr04_t *s, hal_pin_t trig, hal_pin_t echo);
/** Trigger and wait for the echo; distance in millimetres. */
hal_status_t hcsr04_measure_mm(hcsr04_t *s, uint32_t *mm);
/** Echo time -> distance at ~20 C (343 m/s): mm = us * 343 / 2000. */
uint32_t     hcsr04_us_to_mm(uint32_t echo_us);

#endif /* PULSE_H */

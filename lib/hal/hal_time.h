/**
 * @file    hal_time.h
 * @brief   System time base (SysTick at 1 kHz) and delays.
 *
 * Unlike the TP's delay(N) loop of NOPs, these delays are calibrated
 * in real milliseconds/microseconds whatever the CPU clock.
 * All counters are 32-bit and wrap: always compare with subtraction,
 * (now - start) >= duration, never with now >= start + duration.
 */
#ifndef HAL_TIME_H
#define HAL_TIME_H

#include "hal_types.h"

typedef void (*hal_tick_hook_t)(uint32_t now_ms);

/** Start the 1 ms tick. hal_sys_init() must have set the clock first. */
void     hal_time_init(void);
/** Milliseconds since hal_time_init (wraps after 49.7 days). */
uint32_t hal_time_ms(void);
/** Microseconds since hal_time_init (wraps after 71.6 minutes). */
uint32_t hal_time_us(void);
/** Works with interrupts masked too (then as a calibrated busy loop). */
void     hal_delay_ms(uint32_t ms);
/** Exact with interrupts enabled; with interrupts masked keep it < 1 ms
 *  (a SysTick wrap can only be detected once without the ISR). */
void     hal_delay_us(uint32_t us);
/** Function called from the tick interrupt every millisecond
 *  (keep it short: debounce sampling, counters...). NULL to remove. */
void     hal_time_set_tick_hook(hal_tick_hook_t hook);

/** Wrap-safe timeout check. */
static inline bool hal_time_elapsed(uint32_t start_ms, uint32_t duration_ms)
{
    return (hal_time_ms() - start_ms) >= duration_ms;
}

#endif /* HAL_TIME_H */

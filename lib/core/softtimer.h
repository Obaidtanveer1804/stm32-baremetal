/**
 * @file    softtimer.h
 * @brief   Many software timers on top of one millisecond time base.
 *
 * Callbacks run from softtimer_process(), i.e. in the main loop, not
 * in interrupt context, so they may print, take time, etc.
 * The time base is passed in (hal_time_ms() on target, anything on PC).
 */
#ifndef SOFTTIMER_H
#define SOFTTIMER_H

#include <stdint.h>
#include <stdbool.h>

#define SOFTTIMER_MAX        (16U)
#define SOFTTIMER_INVALID    (-1)

typedef void (*softtimer_cb_t)(void *arg);

typedef enum {
    SOFTTIMER_ONE_SHOT = 0,
    SOFTTIMER_PERIODIC
} softtimer_mode_t;

/** Start a timer; returns its id (0..SOFTTIMER_MAX-1) or -1 if full. */
int32_t  softtimer_start(uint32_t now_ms, uint32_t period_ms, softtimer_mode_t mode,
                         softtimer_cb_t cb, void *arg);
void     softtimer_stop(int32_t id);
/** Restart from now with the same period. */
void     softtimer_restart(int32_t id, uint32_t now_ms);
void     softtimer_set_period(int32_t id, uint32_t period_ms);
bool     softtimer_is_active(int32_t id);
/** Milliseconds left before the timer fires (0 if inactive). */
uint32_t softtimer_remaining(int32_t id, uint32_t now_ms);
/** Run expired callbacks. Returns the number of callbacks executed. */
uint32_t softtimer_process(uint32_t now_ms);
void     softtimer_stop_all(void);
uint32_t softtimer_active_count(void);

/* ------------------ polling helper without callbacks -------------- */
/**
 * Non-blocking "every N ms" check, wrap-safe:
 *     static uint32_t t;  if (softtimer_elapsed(&t, now, 500)) toggle();
 */
bool softtimer_elapsed(uint32_t *last_ms, uint32_t now_ms, uint32_t period_ms);

#endif /* SOFTTIMER_H */

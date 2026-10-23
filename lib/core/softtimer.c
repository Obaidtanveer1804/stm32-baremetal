/**
 * @file    softtimer.c
 * @brief   Software timers (see softtimer.h).
 */
#include "softtimer.h"

#include <stddef.h>

typedef struct {
    uint32_t         start_ms;
    uint32_t         period_ms;
    softtimer_cb_t   cb;
    void            *arg;
    softtimer_mode_t mode;
    bool             active;
} softtimer_slot_t;

static softtimer_slot_t s_timers[SOFTTIMER_MAX];

static bool id_valid(int32_t id)
{
    return (id >= 0) && ((uint32_t)id < SOFTTIMER_MAX);
}

int32_t softtimer_start(uint32_t now_ms, uint32_t period_ms, softtimer_mode_t mode,
                        softtimer_cb_t cb, void *arg)
{
    for (uint32_t i = 0U; i < SOFTTIMER_MAX; i++) {
        if (!s_timers[i].active) {
            s_timers[i].start_ms  = now_ms;
            s_timers[i].period_ms = period_ms;
            s_timers[i].cb        = cb;
            s_timers[i].arg       = arg;
            s_timers[i].mode      = mode;
            s_timers[i].active    = true;
            return (int32_t)i;
        }
    }
    return SOFTTIMER_INVALID;
}

void softtimer_stop(int32_t id)
{
    if (id_valid(id)) {
        s_timers[id].active = false;
    }
}

void softtimer_restart(int32_t id, uint32_t now_ms)
{
    if (id_valid(id)) {
        s_timers[id].start_ms = now_ms;
        s_timers[id].active   = true;
    }
}

void softtimer_set_period(int32_t id, uint32_t period_ms)
{
    if (id_valid(id)) {
        s_timers[id].period_ms = period_ms;
    }
}

bool softtimer_is_active(int32_t id)
{
    return id_valid(id) && s_timers[id].active;
}

uint32_t softtimer_remaining(int32_t id, uint32_t now_ms)
{
    if (!softtimer_is_active(id)) {
        return 0U;
    }
    const uint32_t elapsed = now_ms - s_timers[id].start_ms;
    return (elapsed >= s_timers[id].period_ms) ? 0U : (s_timers[id].period_ms - elapsed);
}

uint32_t softtimer_process(uint32_t now_ms)
{
    uint32_t fired = 0U;

    for (uint32_t i = 0U; i < SOFTTIMER_MAX; i++) {
        softtimer_slot_t *t = &s_timers[i];
        if (!t->active || ((now_ms - t->start_ms) < t->period_ms)) {
            continue;
        }
        if (t->mode == SOFTTIMER_PERIODIC) {
            /* advance by exactly one period: no drift accumulation */
            t->start_ms += t->period_ms;
            /* if we are far behind, resynchronise instead of bursting */
            if ((now_ms - t->start_ms) >= t->period_ms) {
                t->start_ms = now_ms;
            }
        } else {
            t->active = false;
        }
        if (t->cb != NULL) {
            t->cb(t->arg);
        }
        fired++;
    }
    return fired;
}

void softtimer_stop_all(void)
{
    for (uint32_t i = 0U; i < SOFTTIMER_MAX; i++) {
        s_timers[i].active = false;
    }
}

uint32_t softtimer_active_count(void)
{
    uint32_t n = 0U;
    for (uint32_t i = 0U; i < SOFTTIMER_MAX; i++) {
        n += s_timers[i].active ? 1U : 0U;
    }
    return n;
}

bool softtimer_elapsed(uint32_t *last_ms, uint32_t now_ms, uint32_t period_ms)
{
    if ((now_ms - *last_ms) >= period_ms) {
        *last_ms += period_ms;
        if ((now_ms - *last_ms) >= period_ms) {
            *last_ms = now_ms;
        }
        return true;
    }
    return false;
}

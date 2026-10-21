/**
 * @file    sched.c
 * @brief   Cooperative scheduler (see sched.h).
 */
#include "sched.h"
#include "strutil.h"

#include <stddef.h>

static sched_task_t   s_tasks[SCHED_MAX_TASKS];
static uint32_t       s_task_count;
static sched_clock_fn s_ms_clock;
static sched_clock_fn s_us_clock;

static bool id_valid(int32_t id)
{
    return (id >= 0) && ((uint32_t)id < s_task_count);
}

void sched_init(sched_clock_fn ms_clock, sched_clock_fn us_clock)
{
    s_ms_clock   = ms_clock;
    s_us_clock   = us_clock;
    s_task_count = 0U;
}

int32_t sched_add(const char *name, sched_task_fn fn, void *arg, uint32_t period_ms)
{
    if ((s_task_count >= SCHED_MAX_TASKS) || (fn == NULL) || (s_ms_clock == NULL)) {
        return -1;
    }
    sched_task_t *t = &s_tasks[s_task_count];
    t->name      = name;
    t->fn        = fn;
    t->arg       = arg;
    t->period_ms = period_ms;
    t->last_ms   = s_ms_clock();
    t->runs      = 0U;
    t->max_us    = 0U;
    t->total_us  = 0U;
    t->enabled   = true;
    return (int32_t)s_task_count++;
}

void sched_enable(int32_t id, bool enable)
{
    if (id_valid(id)) {
        s_tasks[id].enabled = enable;
        s_tasks[id].last_ms = s_ms_clock();
    }
}

void sched_set_period(int32_t id, uint32_t period_ms)
{
    if (id_valid(id)) {
        s_tasks[id].period_ms = period_ms;
    }
}

int32_t sched_find(const char *name)
{
    for (uint32_t i = 0U; i < s_task_count; i++) {
        if (str_eq(s_tasks[i].name, name)) {
            return (int32_t)i;
        }
    }
    return -1;
}

uint32_t sched_run_pending(void)
{
    uint32_t ran = 0U;

    for (uint32_t i = 0U; i < s_task_count; i++) {
        sched_task_t  *t   = &s_tasks[i];
        const uint32_t now = s_ms_clock();

        if (!t->enabled || ((now - t->last_ms) < t->period_ms)) {
            continue;
        }
        t->last_ms += t->period_ms;
        if ((now - t->last_ms) >= t->period_ms) {
            t->last_ms = now;     /* overrun: skip missed slots */
        }

        const uint32_t t0 = (s_us_clock != NULL) ? s_us_clock() : 0U;
        t->fn(t->arg);
        if (s_us_clock != NULL) {
            const uint32_t dt = s_us_clock() - t0;
            t->total_us += dt;
            if (dt > t->max_us) {
                t->max_us = dt;
            }
        }
        t->runs++;
        ran++;
    }
    return ran;
}

void sched_run_forever(void (*idle)(void))
{
    for (;;) {
        (void)sched_run_pending();
        if (idle != NULL) {
            idle();
        }
    }
}

uint32_t sched_task_count(void)
{
    return s_task_count;
}

const sched_task_t *sched_task_get(uint32_t index)
{
    return (index < s_task_count) ? &s_tasks[index] : NULL;
}

void sched_reset_stats(void)
{
    for (uint32_t i = 0U; i < s_task_count; i++) {
        s_tasks[i].runs     = 0U;
        s_tasks[i].max_us   = 0U;
        s_tasks[i].total_us = 0U;
    }
}

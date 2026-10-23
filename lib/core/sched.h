/**
 * @file    sched.h
 * @brief   Cooperative (run-to-completion) task scheduler.
 *
 * Each task is a function called every period_ms. Tasks must return
 * quickly (no blocking delays): they keep their state in static data
 * or in a state machine. This is the classic "super loop with time
 * slots" used on small MCUs instead of an RTOS.
 *
 * The scheduler measures run counts and the worst execution time of
 * each task, which the CLI command "top" displays.
 */
#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stdbool.h>

#define SCHED_MAX_TASKS   (12U)

typedef void     (*sched_task_fn)(void *arg);
typedef uint32_t (*sched_clock_fn)(void);   /**< time source (ms or us) */

typedef struct {
    const char    *name;
    sched_task_fn  fn;
    void          *arg;
    uint32_t       period_ms;
    uint32_t       last_ms;
    uint32_t       runs;
    uint32_t       max_us;       /**< worst-case execution time        */
    uint32_t       total_us;     /**< for the average (wraps, it's ok)  */
    bool           enabled;
} sched_task_t;

/**
 * @param ms_clock  millisecond time source (mandatory)
 * @param us_clock  microsecond time source for profiling (may be NULL)
 */
void     sched_init(sched_clock_fn ms_clock, sched_clock_fn us_clock);
/** Returns the task id, or -1 if the table is full. */
int32_t  sched_add(const char *name, sched_task_fn fn, void *arg, uint32_t period_ms);
void     sched_enable(int32_t id, bool enable);
void     sched_set_period(int32_t id, uint32_t period_ms);
int32_t  sched_find(const char *name);
/** Run every task that is due once. Call it in the main loop. */
uint32_t sched_run_pending(void);
/** Never returns: sched_run_pending() forever, @p idle between rounds. */
void     sched_run_forever(void (*idle)(void));

uint32_t            sched_task_count(void);
const sched_task_t *sched_task_get(uint32_t index);
void                sched_reset_stats(void);

#endif /* SCHED_H */

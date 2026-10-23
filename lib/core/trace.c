/**
 * @file    trace.c
 * @brief   Event trace ring (see trace.h).
 *
 * The core layer has no HAL dependency, so there is no interrupt
 * masking here: a slot is claimed by one increment of a volatile index.
 * An ISR that fires between the claim and the store can at worst swap
 * the order of two events; it can never corrupt the ring.
 */
#include "trace.h"
#include "io.h"
#include "bits.h"

#include <stddef.h>

#define TRACE_NAMES  (16U)

STATIC_ASSERT(IS_POW2(TRACE_DEPTH), trace_depth_pow2);

static trace_event_t     s_ring[TRACE_DEPTH];
static volatile uint32_t s_head;
static trace_clock_fn    s_clock;
static bool              s_enabled = true;
static const char       *s_names[TRACE_NAMES];

void trace_init(trace_clock_fn clock_us)
{
    s_clock = clock_us;
    s_head  = 0U;
}

void trace_enable(bool on)
{
    s_enabled = on;
}

void trace_record(uint16_t id, uint32_t value)
{
    if (!s_enabled) {
        return;
    }
    const uint32_t slot = s_head++;
    trace_event_t *e    = &s_ring[slot & (TRACE_DEPTH - 1U)];
    e->time_us = (s_clock != NULL) ? s_clock() : slot;
    e->id      = id;
    e->value   = value;
}

void trace_name(uint16_t id, const char *name)
{
    if (id < TRACE_NAMES) {
        s_names[id] = name;
    }
}

uint32_t trace_count(void)
{
    return (s_head < TRACE_DEPTH) ? s_head : TRACE_DEPTH;
}

void trace_clear(void)
{
    s_head = 0U;
}

bool trace_get(uint32_t index, trace_event_t *ev)
{
    const uint32_t n = trace_count();
    if (index >= n) {
        return false;
    }
    const uint32_t first = s_head - n;
    *ev = s_ring[(first + index) & (TRACE_DEPTH - 1U)];
    return true;
}

void trace_dump(void)
{
    trace_event_t ev;
    trace_event_t prev = { 0U, 0U, 0U, 0U };
    const uint32_t n = trace_count();

    io_printf("%lu events (of %lu recorded)" IO_NEWLINE, (unsigned long)n, (unsigned long)s_head);
    io_println("      time us     +delta  id  name         value");
    for (uint32_t i = 0U; trace_get(i, &ev); i++) {
        const char *name = ((ev.id < TRACE_NAMES) && (s_names[ev.id] != NULL)) ? s_names[ev.id] : "";
        io_printf("%13lu %10lu  %2u  %-12s %lu (0x%lX)" IO_NEWLINE, (unsigned long)ev.time_us,
                  (unsigned long)((i == 0U) ? 0U : (ev.time_us - prev.time_us)), (unsigned)ev.id,
                  name, (unsigned long)ev.value, (unsigned long)ev.value);
        prev = ev;
    }
}

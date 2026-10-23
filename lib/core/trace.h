/**
 * @file    trace.h
 * @brief   Low-overhead event tracing: record (time, event id, value)
 *          tuples into a RAM ring from anywhere (ISRs included), dump
 *          them later. printf in an ISR changes the timing you want to
 *          observe; a trace costs a few instructions.
 *
 *   TRACE(TRC_ADC_DONE, raw);           // in the ADC ISR
 *   ...
 *   trace_dump();                        // "trace" CLI command
 */
#ifndef TRACE_H
#define TRACE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TRACE_DEPTH
#define TRACE_DEPTH  (64U)        /* power of two */
#endif

typedef struct {
    uint32_t time_us;
    uint16_t id;
    uint16_t pad;
    uint32_t value;
} trace_event_t;

typedef uint32_t (*trace_clock_fn)(void);

void     trace_init(trace_clock_fn clock_us);
void     trace_enable(bool on);
/** Record an event; overwrites the oldest when full. ISR safe. */
void     trace_record(uint16_t id, uint32_t value);
/** Register a name for an id (shown by trace_dump). */
void     trace_name(uint16_t id, const char *name);
uint32_t trace_count(void);
void     trace_clear(void);
/** Print oldest to newest with relative times, through io.h. */
void     trace_dump(void);
bool     trace_get(uint32_t index, trace_event_t *ev);   /**< 0 = oldest */

#ifdef TRACE_DISABLE
#define TRACE(id, value)  ((void)0)
#else
#define TRACE(id, value)  trace_record((uint16_t)(id), (uint32_t)(value))
#endif

#endif /* TRACE_H */

/**
 * @file    evq.h
 * @brief   Event queue: interrupts post small events, the main loop
 *          handles them. Decouples "what happened" from "what to do".
 *
 *   ISR:   evq_post(&q, EV_BUTTON, pin);
 *   main:  evq_event_t e; while (evq_get(&q, &e)) handle(e);
 *
 * Single producer / single consumer without locks (like ringbuf).
 * For several producers (several ISRs) wrap evq_post in a critical
 * section (hal_irq_save / hal_irq_restore).
 */
#ifndef EVQ_H
#define EVQ_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t type;
    uint16_t param;
    uint32_t data;
} evq_event_t;

typedef struct {
    evq_event_t       *slots;
    uint16_t           mask;
    volatile uint16_t  head;
    volatile uint16_t  tail;
    volatile uint32_t  dropped;   /**< events lost because the queue was full */
} evq_t;

/** @param size power of two */
bool     evq_init(evq_t *q, evq_event_t *slots, uint16_t size);
bool     evq_post(evq_t *q, uint16_t type, uint16_t param, uint32_t data);
bool     evq_get(evq_t *q, evq_event_t *ev);
uint16_t evq_count(const evq_t *q);
void     evq_flush(evq_t *q);

#endif /* EVQ_H */

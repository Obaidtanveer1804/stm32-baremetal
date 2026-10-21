/**
 * @file    evq.c
 * @brief   Event queue (see evq.h).
 */
#include "evq.h"
#include "bits.h"

#include <stddef.h>

bool evq_init(evq_t *q, evq_event_t *slots, uint16_t size)
{
    if ((slots == NULL) || (size < 2U) || !IS_POW2(size)) {
        return false;
    }
    q->slots   = slots;
    q->mask    = (uint16_t)(size - 1U);
    q->head    = 0U;
    q->tail    = 0U;
    q->dropped = 0U;
    return true;
}

uint16_t evq_count(const evq_t *q)
{
    return (uint16_t)(q->head - q->tail);
}

bool evq_post(evq_t *q, uint16_t type, uint16_t param, uint32_t data)
{
    if (evq_count(q) > q->mask) {
        q->dropped++;
        return false;
    }
    evq_event_t *e = &q->slots[q->head & q->mask];
    e->type  = type;
    e->param = param;
    e->data  = data;
    q->head++;          /* publish after the slot is complete */
    return true;
}

bool evq_get(evq_t *q, evq_event_t *ev)
{
    if (q->head == q->tail) {
        return false;
    }
    *ev = q->slots[q->tail & q->mask];
    q->tail++;
    return true;
}

void evq_flush(evq_t *q)
{
    q->tail = q->head;
}

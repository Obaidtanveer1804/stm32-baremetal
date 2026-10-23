/**
 * @file    debounce.c
 * @brief   Vertical-counter debounce and button gestures (see debounce.h).
 */
#include "debounce.h"

#include <stddef.h>

/* ---------------- vertical counter (32 inputs at once) ------------ */

void debounce_vc_init(debounce_vc_t *d, uint32_t initial_state)
{
    d->state    = initial_state;
    d->cnt0     = 0xFFFFFFFFUL;
    d->cnt1     = 0xFFFFFFFFUL;
    d->pressed  = 0U;
    d->released = 0U;
}

uint32_t debounce_vc_update(debounce_vc_t *d, uint32_t raw)
{
    /* bits whose raw value differs from the stable state */
    uint32_t delta = raw ^ d->state;

    /* 2-bit counter per bit (P. Dannegger's trick): counts 3,2,1,0,3
     * while the input disagrees, held at 3 while it agrees */
    d->cnt0 = ~(d->cnt0 & delta);
    d->cnt1 = d->cnt0 ^ (d->cnt1 & delta);

    /* a bit toggles when its counter rolls over to 3 (4th sample) */
    const uint32_t toggle = delta & d->cnt0 & d->cnt1;
    d->state    ^= toggle;
    d->pressed  |= toggle & d->state;
    d->released |= toggle & ~d->state;
    return toggle;
}

uint32_t debounce_vc_take_pressed(debounce_vc_t *d, uint32_t mask)
{
    const uint32_t ev = d->pressed & mask;
    d->pressed &= ~ev;
    return ev;
}

uint32_t debounce_vc_take_released(debounce_vc_t *d, uint32_t mask)
{
    const uint32_t ev = d->released & mask;
    d->released &= ~ev;
    return ev;
}

/* -------------------- button gesture detector --------------------- */

const btn_config_t btn_config_default = {
    .debounce_ms = 20U,
    .double_ms   = 300U,
    .long_ms     = 800U,
    .repeat_ms   = 150U,
};

void btn_init(btn_t *b, const btn_config_t *cfg)
{
    b->cfg           = (cfg != NULL) ? cfg : &btn_config_default;
    b->raw_last      = false;
    b->stable        = false;
    b->raw_change_ms = 0U;
    b->press_ms      = 0U;
    b->release_ms    = 0U;
    b->repeat_ms     = 0U;
    b->clicks        = 0U;
    b->long_fired    = false;
}

/* (now - then) works across the 49-day wrap of a 32-bit ms counter */
static uint32_t elapsed(uint32_t now, uint32_t then)
{
    return now - then;
}

btn_event_t btn_update(btn_t *b, bool raw_pressed, uint32_t now_ms)
{
    const btn_config_t *cfg = b->cfg;

    /* 1. debounce: the raw level must be stable for debounce_ms */
    if (raw_pressed != b->raw_last) {
        b->raw_last      = raw_pressed;
        b->raw_change_ms = now_ms;
    }
    if ((raw_pressed != b->stable) &&
        (elapsed(now_ms, b->raw_change_ms) >= cfg->debounce_ms)) {
        b->stable = raw_pressed;
        if (b->stable) {
            b->press_ms   = now_ms;
            b->long_fired = false;
            return BTN_EVENT_PRESS;
        }
        b->release_ms = now_ms;
        if (!b->long_fired) {
            b->clicks++;
            if (b->clicks >= 2U) {
                b->clicks = 0U;
                return BTN_EVENT_DOUBLE_CLICK;
            }
        }
        return BTN_EVENT_RELEASE;
    }

    /* 2. while held: long press then auto-repeat */
    if (b->stable) {
        if (!b->long_fired && (elapsed(now_ms, b->press_ms) >= cfg->long_ms)) {
            b->long_fired = true;
            b->clicks     = 0U;
            b->repeat_ms  = now_ms;
            return BTN_EVENT_LONG_PRESS;
        }
        if (b->long_fired && (cfg->repeat_ms != 0U) &&
            (elapsed(now_ms, b->repeat_ms) >= cfg->repeat_ms)) {
            b->repeat_ms = now_ms;
            return BTN_EVENT_REPEAT;
        }
        return BTN_EVENT_NONE;
    }

    /* 3. released: a single click is confirmed once the double window
     *    expired without a second press */
    if ((b->clicks == 1U) && (elapsed(now_ms, b->release_ms) >= cfg->double_ms)) {
        b->clicks = 0U;
        return BTN_EVENT_CLICK;
    }
    return BTN_EVENT_NONE;
}

const char *btn_event_name(btn_event_t ev)
{
    static const char *const names[] = {
        "none", "press", "release", "click", "double", "long", "repeat"
    };
    return ((uint32_t)ev < (sizeof(names) / sizeof(names[0]))) ? names[ev] : "?";
}

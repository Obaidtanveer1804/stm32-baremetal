/**
 * @file    debounce.h
 * @brief   Switch debouncing: 32 inputs in parallel with vertical
 *          counters, plus a per-button click / double / long detector.
 *
 * Hardware independent: you pass the raw input bits and the current
 * time in ms; the caller decides where they come from (GPIO IDR on
 * the MCU, keyboard or a test vector on the PC).
 */
#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdint.h>
#include <stdbool.h>

/* ---------------- vertical counter (32 inputs at once) ------------ */
/**
 * Each input bit has a 2-bit counter spread over cnt0/cnt1. The stable
 * state of a bit changes only after 4 consecutive samples disagreeing
 * with it. Sample every 5..10 ms (e.g. from a timer interrupt).
 * Cost: ~8 logic operations for all 32 inputs together.
 */
typedef struct {
    uint32_t state;      /**< debounced level of every input           */
    uint32_t cnt0;       /**< counter bit 0 of every input             */
    uint32_t cnt1;       /**< counter bit 1 of every input             */
    uint32_t pressed;    /**< latched 0->1 edges, cleared on read      */
    uint32_t released;   /**< latched 1->0 edges, cleared on read      */
} debounce_vc_t;

void     debounce_vc_init(debounce_vc_t *d, uint32_t initial_state);
/** Feed one raw sample (1 = active). Returns the bits that changed. */
uint32_t debounce_vc_update(debounce_vc_t *d, uint32_t raw);
/** Read-and-clear edge masks (safe to call from the main loop). */
uint32_t debounce_vc_take_pressed(debounce_vc_t *d, uint32_t mask);
uint32_t debounce_vc_take_released(debounce_vc_t *d, uint32_t mask);

/* -------------------- button gesture detector --------------------- */
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_PRESS,         /**< debounced press (immediate)           */
    BTN_EVENT_RELEASE,
    BTN_EVENT_CLICK,         /**< short press, no second click followed */
    BTN_EVENT_DOUBLE_CLICK,
    BTN_EVENT_LONG_PRESS,    /**< held for long_ms (fires once)         */
    BTN_EVENT_REPEAT         /**< auto-repeat while held after long     */
} btn_event_t;

typedef struct {
    uint16_t debounce_ms;
    uint16_t double_ms;      /**< max gap between two clicks            */
    uint16_t long_ms;
    uint16_t repeat_ms;      /**< 0 disables auto-repeat                */
} btn_config_t;

typedef struct {
    const btn_config_t *cfg;
    bool     raw_last;
    bool     stable;
    uint32_t raw_change_ms;
    uint32_t press_ms;
    uint32_t release_ms;
    uint32_t repeat_ms;
    uint8_t  clicks;
    bool     long_fired;
} btn_t;

extern const btn_config_t btn_config_default;  /* 20 / 300 / 800 / 150 ms */

void        btn_init(btn_t *b, const btn_config_t *cfg);
/** Call often (every 1..10 ms) with the raw level and the time. */
btn_event_t btn_update(btn_t *b, bool raw_pressed, uint32_t now_ms);
const char *btn_event_name(btn_event_t ev);

#endif /* DEBOUNCE_H */

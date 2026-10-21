/**
 * @file    traffic_light.c
 * @brief   Pedestrian crossing controller: a table-driven state machine
 *          (fsm.h) fed by events from a button (EXTI interrupt + event
 *          queue) and software timers. No delay() anywhere: the main
 *          loop only dispatches events, so the CPU sleeps (WFI) between
 *          them.
 *
 *   car lights : PB12 red, PB13 amber, PB14 green
 *   walk light : PC9 (green LED), USER button PA0 = request to cross
 *
 * PC: "make run APP=traffic_light", press Enter to push the button.
 */
#include "hal.h"
#include "io.h"
#include "log.h"
#include "fsm.h"
#include "evq.h"
#include "softtimer.h"

#define PIN_CAR_RED     PB(12)
#define PIN_CAR_AMBER   PB(13)
#define PIN_CAR_GREEN   PB(14)
#define PIN_WALK        BOARD_LED_GREEN

#define T_GREEN_MIN_MS  (4000U)    /* cars keep green at least this long */
#define T_AMBER_MS      (1500U)
#define T_ALL_RED_MS    (800U)
#define T_WALK_MS       (4000U)
#define T_CLEAR_MS      (2000U)    /* walk light blinks */

enum {
    ST_CAR_GREEN = 0,
    ST_CAR_GREEN_REQ,     /* request pending, waiting for min green */
    ST_CAR_AMBER,
    ST_ALL_RED,
    ST_WALK,
    ST_WALK_CLEAR,
    ST_COUNT
};

enum {
    EV_BUTTON = 0,
    EV_TIMEOUT,
    EV_BLINK
};

static evq_event_t s_slots[8];
static evq_t       s_events;
static fsm_t       s_fsm;
static int32_t     s_timer = SOFTTIMER_INVALID;
static int32_t     s_blink = SOFTTIMER_INVALID;
static bool        s_green_elapsed;

/* ----------------------------- outputs ---------------------------- */

static void cars(bool red, bool amber, bool green)
{
    /* one BSRR write per call: the three LEDs change together */
    const uint16_t set = (uint16_t)((red ? HAL_PIN_MASK(PIN_CAR_RED) : 0U) |
                                    (amber ? HAL_PIN_MASK(PIN_CAR_AMBER) : 0U) |
                                    (green ? HAL_PIN_MASK(PIN_CAR_GREEN) : 0U));
    const uint16_t all = (uint16_t)(HAL_PIN_MASK(PIN_CAR_RED) | HAL_PIN_MASK(PIN_CAR_AMBER) |
                                    HAL_PIN_MASK(PIN_CAR_GREEN));
    hal_gpio_port_write(HAL_PIN_PORT(PIN_CAR_RED), set, (uint16_t)(all & (uint16_t)~set));
}

static void on_timer(void *arg)
{
    (void)evq_post(&s_events, (uint16_t)(uintptr_t)arg, 0U, 0U);
}

static void arm(uint32_t ms)
{
    softtimer_stop(s_timer);
    s_timer = softtimer_start(hal_time_ms(), ms, SOFTTIMER_ONE_SHOT, on_timer, (void *)(uintptr_t)EV_TIMEOUT);
}

/* ------------------------- state entry hooks ---------------------- */

static void enter_green(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    cars(false, false, true);
    hal_gpio_clear(PIN_WALK);
    s_green_elapsed = false;
    arm(T_GREEN_MIN_MS);
}

static void enter_green_req(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    if (s_green_elapsed) {
        (void)evq_post(&s_events, EV_TIMEOUT, 0U, 0U);   /* go now */
    }
}

static void enter_amber(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    cars(false, true, false);
    arm(T_AMBER_MS);
}

static void enter_all_red(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    cars(true, false, false);
    arm(T_ALL_RED_MS);
}

static void enter_walk(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    hal_gpio_set(PIN_WALK);
    arm(T_WALK_MS);
}

static void enter_clear(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    s_blink = softtimer_start(hal_time_ms(), 250U, SOFTTIMER_PERIODIC, on_timer, (void *)(uintptr_t)EV_BLINK);
    arm(T_CLEAR_MS);
}

static void exit_clear(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    softtimer_stop(s_blink);
}

static void blink_walk(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    hal_gpio_toggle(PIN_WALK);
}

static void note_green_done(fsm_t *m, fsm_event_t ev)
{
    (void)m;
    (void)ev;
    s_green_elapsed = true;
}

static void trace(const fsm_t *m, fsm_state_t from, fsm_event_t ev, fsm_state_t to)
{
    static const char *const ev_names[] = { "button", "timeout", "blink" };
    if (ev != EV_BLINK) {
        LOG_I("fsm", "%-10s --%s--> %s", fsm_state_name(m, from), ev_names[ev], fsm_state_name(m, to));
    }
}

/* -------------------------- the behaviour ------------------------- */

static const fsm_state_info_t states[ST_COUNT] = {
    [ST_CAR_GREEN]     = { "green",     enter_green,     NULL },
    [ST_CAR_GREEN_REQ] = { "green+req", enter_green_req, NULL },
    [ST_CAR_AMBER]     = { "amber",     enter_amber,     NULL },
    [ST_ALL_RED]       = { "all-red",   enter_all_red,   NULL },
    [ST_WALK]          = { "walk",      enter_walk,      NULL },
    [ST_WALK_CLEAR]    = { "clearing",  enter_clear,     exit_clear },
};

static const fsm_transition_t table[] = {
    { ST_CAR_GREEN,     EV_BUTTON,  ST_CAR_GREEN_REQ, NULL },
    { ST_CAR_GREEN,     EV_TIMEOUT, ST_CAR_GREEN,     note_green_done },
    { ST_CAR_GREEN_REQ, EV_TIMEOUT, ST_CAR_AMBER,     NULL },
    { ST_CAR_AMBER,     EV_TIMEOUT, ST_ALL_RED,       NULL },
    { ST_ALL_RED,       EV_TIMEOUT, ST_WALK,          NULL },
    { ST_WALK,          EV_TIMEOUT, ST_WALK_CLEAR,    NULL },
    { ST_WALK_CLEAR,    EV_BLINK,   ST_WALK_CLEAR,    blink_walk },
    { ST_WALK_CLEAR,    EV_TIMEOUT, ST_CAR_GREEN,     NULL },
    /* button presses in other states are simply ignored (no row) */
};

/* --------------------------- interrupts --------------------------- */

static void button_isr(hal_pin_t pin, void *arg)
{
    (void)pin;
    (void)arg;
    (void)evq_post(&s_events, EV_BUTTON, 0U, hal_time_ms());
}

int main(void)
{
    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    log_init(hal_time_ms);

    hal_gpio_port_mode(HAL_PIN_PORT(PIN_CAR_RED),
                       (uint16_t)(HAL_PIN_MASK(PIN_CAR_RED) | HAL_PIN_MASK(PIN_CAR_AMBER) | HAL_PIN_MASK(PIN_CAR_GREEN)),
                       HAL_GPIO_OUTPUT);
    hal_gpio_mode(PIN_WALK, HAL_GPIO_OUTPUT);
    hal_gpio_mode(BOARD_BUTTON, (BOARD_BUTTON_ACTIVE_HIGH != 0) ? HAL_GPIO_INPUT_PULLDOWN : HAL_GPIO_INPUT_PULLUP);
    (void)hal_exti_attach(BOARD_BUTTON, (BOARD_BUTTON_ACTIVE_HIGH != 0) ? HAL_EXTI_RISING : HAL_EXTI_FALLING,
                          button_isr, NULL);
    (void)evq_init(&s_events, s_slots, 8U);

    io_println("pedestrian crossing: press the button (Enter on the PC) to cross");
    fsm_init(&s_fsm, table, sizeof(table) / sizeof(table[0]), states, ST_COUNT, ST_CAR_GREEN, NULL);
    s_fsm.trace = trace;

    for (;;) {
        evq_event_t ev;
        (void)softtimer_process(hal_time_ms());
        while (evq_get(&s_events, &ev)) {
            (void)fsm_dispatch(&s_fsm, (fsm_event_t)ev.type);
        }
        /* console: Enter acts as the button (handy on the PC) */
        const int c = io_getc();
        if ((c == '\r') || (c == '\n')) {
            (void)evq_post(&s_events, EV_BUTTON, 0U, 0U);
        }
        hal_sys_idle();
    }
    return 0;
}

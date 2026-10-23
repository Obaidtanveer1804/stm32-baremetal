/**
 * @file    fsm.h
 * @brief   Table-driven finite state machine.
 *
 * The behaviour is data, not nested switch/case:
 *
 *   static const fsm_transition_t table[] = {
 *       { ST_IDLE,    EV_BUTTON,  ST_RUNNING, start_motor },
 *       { ST_RUNNING, EV_BUTTON,  ST_IDLE,    stop_motor  },
 *       { FSM_ANY,    EV_FAULT,   ST_ERROR,   alarm_on    },
 *   };
 *
 * Optional on_enter / on_exit hooks per state through a state table.
 */
#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdbool.h>

#define FSM_ANY   (0xFFU)    /**< wildcard "from any state"        */

typedef uint8_t fsm_state_t;
typedef uint8_t fsm_event_t;

struct fsm;
typedef void (*fsm_action_fn)(struct fsm *m, fsm_event_t ev);

typedef struct {
    fsm_state_t   from;      /**< state or FSM_ANY                  */
    fsm_event_t   event;
    fsm_state_t   to;
    fsm_action_fn action;    /**< may be NULL                       */
} fsm_transition_t;

typedef struct {
    const char    *name;     /**< for traces, may be NULL           */
    fsm_action_fn  on_enter; /**< may be NULL                       */
    fsm_action_fn  on_exit;  /**< may be NULL                       */
} fsm_state_info_t;

typedef struct fsm {
    const fsm_transition_t *table;
    uint32_t                count;
    const fsm_state_info_t *states;     /**< optional, indexed by state */
    uint32_t                nstates;
    fsm_state_t             current;
    fsm_state_t             previous;
    uint32_t                transitions;
    void                   *user;       /**< application context        */
    void (*trace)(const struct fsm *m, fsm_state_t from, fsm_event_t ev, fsm_state_t to);
} fsm_t;

void        fsm_init(fsm_t *m, const fsm_transition_t *table, uint32_t count,
                     const fsm_state_info_t *states, uint32_t nstates,
                     fsm_state_t initial, void *user);
/** Process one event. Returns true if a transition matched. */
bool        fsm_dispatch(fsm_t *m, fsm_event_t ev);
fsm_state_t fsm_state(const fsm_t *m);
const char *fsm_state_name(const fsm_t *m, fsm_state_t s);

#endif /* FSM_H */

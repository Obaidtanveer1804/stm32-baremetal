/**
 * @file    fsm.c
 * @brief   Table-driven state machine (see fsm.h).
 */
#include "fsm.h"

#include <stddef.h>

void fsm_init(fsm_t *m, const fsm_transition_t *table, uint32_t count,
              const fsm_state_info_t *states, uint32_t nstates,
              fsm_state_t initial, void *user)
{
    m->table       = table;
    m->count       = count;
    m->states      = states;
    m->nstates     = nstates;
    m->current     = initial;
    m->previous    = initial;
    m->transitions = 0U;
    m->user        = user;
    m->trace       = NULL;

    if ((states != NULL) && (initial < nstates) && (states[initial].on_enter != NULL)) {
        states[initial].on_enter(m, 0U);
    }
}

static const fsm_state_info_t *info(const fsm_t *m, fsm_state_t s)
{
    return ((m->states != NULL) && (s < m->nstates)) ? &m->states[s] : NULL;
}

bool fsm_dispatch(fsm_t *m, fsm_event_t ev)
{
    for (uint32_t i = 0U; i < m->count; i++) {
        const fsm_transition_t *t = &m->table[i];

        if ((t->event != ev) || ((t->from != FSM_ANY) && (t->from != m->current))) {
            continue;
        }

        const fsm_state_t from     = m->current;
        const bool        changing = (t->to != from);
        const fsm_state_info_t *old_info = info(m, from);
        const fsm_state_info_t *new_info = info(m, t->to);

        if (changing && (old_info != NULL) && (old_info->on_exit != NULL)) {
            old_info->on_exit(m, ev);
        }
        if (t->action != NULL) {
            t->action(m, ev);
        }
        m->previous = from;
        m->current  = t->to;
        m->transitions++;
        if (changing && (new_info != NULL) && (new_info->on_enter != NULL)) {
            new_info->on_enter(m, ev);
        }
        if (m->trace != NULL) {
            m->trace(m, from, ev, t->to);
        }
        return true;    /* first matching row wins */
    }
    return false;
}

fsm_state_t fsm_state(const fsm_t *m)
{
    return m->current;
}

const char *fsm_state_name(const fsm_t *m, fsm_state_t s)
{
    const fsm_state_info_t *i = info(m, s);
    return ((i != NULL) && (i->name != NULL)) ? i->name : "?";
}

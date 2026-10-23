/**
 * @file    menu.c
 * @brief   Menu navigation (see menu.h).
 */
#include "menu.h"
#include "fmt.h"

#include <stddef.h>

static uint32_t item_count(const menu_item_t *list)
{
    uint32_t n = 0U;
    while (list[n].type != MENU_ITEM_END) {
        n++;
    }
    return n;
}

void menu_init(menu_t *m, const menu_item_t *root, uint8_t rows, menu_draw_fn draw, void *ctx)
{
    m->stack[0]  = root;
    m->cursor[0] = 0U;
    m->depth     = 0U;
    m->rows      = (rows == 0U) ? 1U : rows;
    m->scroll    = 0U;
    m->editing   = false;
    m->draw      = draw;
    m->ctx       = ctx;
}

const menu_item_t *menu_current(const menu_t *m)
{
    return &m->stack[m->depth][m->cursor[m->depth]];
}

void menu_item_text(const menu_item_t *it, bool editing, char *buf, uint32_t size)
{
    switch (it->type) {
    case MENU_ITEM_SUBMENU:
        (void)fmt_snprintf(buf, size, "%s >", it->label);
        break;
    case MENU_ITEM_VALUE:
        (void)fmt_snprintf(buf, size, editing ? "%s [%ld%s]" : "%s %ld%s", it->label,
                           (long)*it->value, (it->unit != NULL) ? it->unit : "");
        break;
    case MENU_ITEM_TOGGLE:
        (void)fmt_snprintf(buf, size, "%s %s", it->label, *it->flag ? "on" : "off");
        break;
    case MENU_ITEM_ACTION:
    case MENU_ITEM_END:
    default:
        (void)fmt_snprintf(buf, size, "%s", (it->label != NULL) ? it->label : "");
        break;
    }
}

static void clamp_scroll(menu_t *m)
{
    const uint8_t c = m->cursor[m->depth];
    if (c < m->scroll) {
        m->scroll = c;
    } else if (c >= (m->scroll + m->rows)) {
        m->scroll = (uint8_t)(c - m->rows + 1U);
    } else {
        /* cursor visible */
    }
}

static void edit_value(const menu_item_t *it, int32_t dir)
{
    const int32_t step = (it->step == 0) ? 1 : it->step;
    int32_t       v    = *it->value + (dir * step);
    v = (v < it->min) ? it->min : ((v > it->max) ? it->max : v);
    *it->value = v;
}

bool menu_key(menu_t *m, menu_key_t key)
{
    const menu_item_t *list  = m->stack[m->depth];
    const uint32_t     n     = item_count(list);
    uint8_t           *cur   = &m->cursor[m->depth];
    const menu_item_t *it    = &list[*cur];

    if (n == 0U) {
        return false;
    }
    if (m->editing) {
        switch (key) {
        case MENU_KEY_UP:   edit_value(it, +1); return true;
        case MENU_KEY_DOWN: edit_value(it, -1); return true;
        case MENU_KEY_SELECT:
        case MENU_KEY_BACK:
        default:            m->editing = false; return true;
        }
    }
    switch (key) {
    case MENU_KEY_UP:
        *cur = (uint8_t)((*cur == 0U) ? (n - 1U) : (*cur - 1U));   /* wrap around */
        break;
    case MENU_KEY_DOWN:
        *cur = (uint8_t)(((uint32_t)*cur + 1U) % n);
        break;
    case MENU_KEY_SELECT:
        if ((it->type == MENU_ITEM_SUBMENU) && (m->depth < (MENU_MAX_DEPTH - 1U))) {
            m->depth++;
            m->stack[m->depth]  = it->sub;
            m->cursor[m->depth] = 0U;
            m->scroll           = 0U;
        } else if (it->type == MENU_ITEM_ACTION) {
            if (it->action != NULL) {
                it->action();
            }
        } else if (it->type == MENU_ITEM_TOGGLE) {
            *it->flag = !*it->flag;
        } else if (it->type == MENU_ITEM_VALUE) {
            m->editing = true;
        } else {
            return false;
        }
        break;
    case MENU_KEY_BACK:
        if (m->depth == 0U) {
            return false;
        }
        m->depth--;
        m->scroll = 0U;
        break;
    default:
        return false;
    }
    clamp_scroll(m);
    return true;
}

void menu_render(menu_t *m)
{
    char               line[MENU_LINE_MAX];
    const menu_item_t *list = m->stack[m->depth];
    const uint32_t     n    = item_count(list);

    clamp_scroll(m);
    for (uint32_t row = 0U; row < m->rows; row++) {
        const uint32_t idx = (uint32_t)m->scroll + row;
        if (idx < n) {
            const bool sel = (idx == m->cursor[m->depth]);
            menu_item_text(&list[idx], sel && m->editing, line, sizeof(line));
            m->draw(row, line, sel, m->ctx);
        } else {
            m->draw(row, "", false, m->ctx);
        }
    }
}

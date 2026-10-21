/**
 * @file    menu.h
 * @brief   Hierarchical menu for small screens (16x2 LCD, OLED, or a
 *          terminal) driven by 4 inputs: up, down, select, back
 *          (a rotary encoder + push button, or 4 buttons).
 *
 * Menus are const tables in flash; only the navigation state is RAM:
 *
 *   static int32_t brightness = 50;
 *   static const menu_item_t settings[] = {
 *       MENU_VALUE("Brightness", &brightness, 0, 100, 5, "%"),
 *       MENU_TOGGLE("Beep", &beep_on),
 *       MENU_ACTION("Factory reset", do_reset),
 *       MENU_END
 *   };
 *   static const menu_item_t root[] = { MENU_SUB("Settings", settings), MENU_END };
 *
 * Rendering goes through a callback that receives each visible line,
 * so the same menu draws on any display.
 */
#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

#define MENU_MAX_DEPTH  (4U)
#define MENU_LINE_MAX   (24U)

typedef enum {
    MENU_ITEM_END = 0,
    MENU_ITEM_SUBMENU,
    MENU_ITEM_ACTION,
    MENU_ITEM_VALUE,
    MENU_ITEM_TOGGLE
} menu_item_type_t;

typedef struct menu_item {
    const char             *label;
    menu_item_type_t        type;
    const struct menu_item *sub;        /* SUBMENU                    */
    void                  (*action)(void);
    int32_t                *value;      /* VALUE                      */
    bool                   *flag;       /* TOGGLE                     */
    int32_t                 min;
    int32_t                 max;
    int32_t                 step;
    const char             *unit;
} menu_item_t;

#define MENU_SUB(l, s)                 { (l), MENU_ITEM_SUBMENU, (s), NULL, NULL, NULL, 0, 0, 0, NULL }
#define MENU_ACTION(l, fn)             { (l), MENU_ITEM_ACTION, NULL, (fn), NULL, NULL, 0, 0, 0, NULL }
#define MENU_VALUE(l, v, lo, hi, st, u) { (l), MENU_ITEM_VALUE, NULL, NULL, (v), NULL, (lo), (hi), (st), (u) }
#define MENU_TOGGLE(l, f)              { (l), MENU_ITEM_TOGGLE, NULL, NULL, NULL, (f), 0, 0, 0, NULL }
#define MENU_END                       { NULL, MENU_ITEM_END, NULL, NULL, NULL, NULL, 0, 0, 0, NULL }

typedef enum {
    MENU_KEY_UP = 0,
    MENU_KEY_DOWN,
    MENU_KEY_SELECT,
    MENU_KEY_BACK
} menu_key_t;

/** Draw one line (row 0 = top); selected = highlight it. */
typedef void (*menu_draw_fn)(uint32_t row, const char *text, bool selected, void *ctx);

typedef struct {
    const menu_item_t *stack[MENU_MAX_DEPTH];
    uint8_t            cursor[MENU_MAX_DEPTH];
    uint8_t            depth;
    uint8_t            rows;        /**< visible lines on the display */
    uint8_t            scroll;
    bool               editing;     /**< changing a VALUE item         */
    menu_draw_fn       draw;
    void              *ctx;
} menu_t;

void menu_init(menu_t *m, const menu_item_t *root, uint8_t rows, menu_draw_fn draw, void *ctx);
/** Process one key. Returns true if the screen must be redrawn. */
bool menu_key(menu_t *m, menu_key_t key);
void menu_render(menu_t *m);
const menu_item_t *menu_current(const menu_t *m);
/** Text of an item as displayed ("Brightness  50%"). */
void menu_item_text(const menu_item_t *it, bool editing, char *buf, uint32_t size);

#endif /* MENU_H */

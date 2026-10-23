/**
 * @file    keypad.h
 * @brief   Matrix keypad (4x4 membrane, 4x3 phone style) scanned by
 *          driving one row low at a time and reading the columns
 *          (pull-ups). Debounced with the vertical counter, so several
 *          keys pressed together are reported correctly (with diodes).
 */
#ifndef KEYPAD_H
#define KEYPAD_H

#include "hal_gpio.h"
#include "debounce.h"

#define KEYPAD_MAX_ROWS  (4U)
#define KEYPAD_MAX_COLS  (4U)
#define KEYPAD_NO_KEY    ('\0')

typedef struct {
    hal_pin_t     rows[KEYPAD_MAX_ROWS];
    hal_pin_t     cols[KEYPAD_MAX_COLS];
    uint8_t       nrows;
    uint8_t       ncols;
    const char   *keymap;            /**< nrows * ncols characters */
    debounce_vc_t deb;
} keypad_t;

/** Standard maps: "123A456B789C*0#D" (4x4) and "123456789*0#" (4x3). */
extern const char keypad_map_4x4[];
extern const char keypad_map_4x3[];

void     keypad_init(keypad_t *k, const hal_pin_t *rows, uint8_t nrows,
                     const hal_pin_t *cols, uint8_t ncols, const char *keymap);
/** Raw scan: bit (r * ncols + c) set if that key is down. */
uint32_t keypad_scan(keypad_t *k);
/** Call every 5..10 ms: returns the next newly pressed key, or '\0'. */
char     keypad_update(keypad_t *k);
/** Keys currently held (debounced bit mask). */
uint32_t keypad_held(const keypad_t *k);

#endif /* KEYPAD_H */

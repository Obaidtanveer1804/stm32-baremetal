/**
 * @file    keypad.c
 * @brief   Matrix keypad scanner (see keypad.h).
 */
#include "keypad.h"
#include "hal_time.h"
#include "bits.h"

#define KEYPAD_SETTLE_US  (5U)    /* line capacitance needs a moment */

const char keypad_map_4x4[] = "123A456B789C*0#D";
const char keypad_map_4x3[] = "123456789*0#";

void keypad_init(keypad_t *k, const hal_pin_t *rows, uint8_t nrows,
                 const hal_pin_t *cols, uint8_t ncols, const char *keymap)
{
    k->nrows  = (nrows > KEYPAD_MAX_ROWS) ? (uint8_t)KEYPAD_MAX_ROWS : nrows;
    k->ncols  = (ncols > KEYPAD_MAX_COLS) ? (uint8_t)KEYPAD_MAX_COLS : ncols;
    k->keymap = keymap;
    for (uint32_t r = 0U; r < k->nrows; r++) {
        k->rows[r] = rows[r];
        /* rows: open drain, released (high) when not scanned. Two keys
         * in one column then cannot short two driven outputs. */
        hal_gpio_set(rows[r]);
        hal_gpio_mode(rows[r], HAL_GPIO_OUTPUT_OD);
    }
    for (uint32_t c = 0U; c < k->ncols; c++) {
        k->cols[c] = cols[c];
        hal_gpio_mode(cols[c], HAL_GPIO_INPUT_PULLUP);
    }
    debounce_vc_init(&k->deb, 0U);
}

uint32_t keypad_scan(keypad_t *k)
{
    uint32_t down = 0U;
    for (uint32_t r = 0U; r < k->nrows; r++) {
        hal_gpio_clear(k->rows[r]);
        hal_delay_us(KEYPAD_SETTLE_US);
        for (uint32_t c = 0U; c < k->ncols; c++) {
            if (!hal_gpio_read(k->cols[c])) {          /* pressed = pulled low */
                down |= BIT((r * k->ncols) + c);
            }
        }
        hal_gpio_set(k->rows[r]);
    }
    return down;
}

char keypad_update(keypad_t *k)
{
    (void)debounce_vc_update(&k->deb, keypad_scan(k));
    const uint32_t pressed = debounce_vc_take_pressed(&k->deb, 0xFFFFFFFFU);
    if (pressed == 0U) {
        return KEYPAD_NO_KEY;
    }
    const uint32_t idx = bits_ctz(pressed);
    /* report the others on the next calls */
    k->deb.pressed |= pressed & ~BIT(idx);
    return k->keymap[idx];
}

uint32_t keypad_held(const keypad_t *k)
{
    return k->deb.state;
}

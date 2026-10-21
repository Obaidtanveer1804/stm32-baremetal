/**
 * @file    menu_demo.c
 * @brief   Settings menu on an SSD1306 OLED, driven by a rotary
 *          encoder (KY-040) with push button, values saved in flash.
 *
 *   OLED I2C1 PB6/PB7, encoder A PB4 / B PB5 / switch PB3
 *   console keys also work: w/s = up/down, Enter = select, b = back
 *
 * Shows menu.h (navigation), encoder.h (quadrature in a 1 kHz timer
 * interrupt), button.h (debounce), config.h (persistent settings).
 */
#include "hal.h"
#include "io.h"
#include "menu.h"
#include "encoder.h"
#include "button.h"
#include "config.h"
#include "ssd1306.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define ENC_A          PB(4)
#define ENC_B          PB(5)
#define ENC_SW         PB(3)
#define CFG_BRIGHT     (1U)
#define CFG_BLINK      (2U)
#define CFG_SOUND      (3U)
#define CFG_RATE       (4U)
#define MENU_ROWS      (5U)

static int32_t   s_brightness;
static int32_t   s_blink_ms;
static int32_t   s_rate;
static bool      s_sound;
static encoder_t s_enc;
static button_t  s_sw;
static ssd1306_t s_oled;
static bool      s_oled_ok;

static void save(void)
{
    (void)config_set_u32(CFG_BRIGHT, (uint32_t)s_brightness);
    (void)config_set_u32(CFG_BLINK, (uint32_t)s_blink_ms);
    (void)config_set_u32(CFG_SOUND, s_sound ? 1U : 0U);
    (void)config_set_u32(CFG_RATE, (uint32_t)s_rate);
    io_println("[saved to flash]");
}

static void factory(void)
{
    s_brightness = 50;
    s_blink_ms   = 500;
    s_sound      = true;
    s_rate       = 9600;
    save();
}

static void about(void)
{
    io_printf("[%s, %s]" IO_NEWLINE, BOARD_NAME, hal_sys_name());
}

static const menu_item_t display_menu[] = {
    MENU_VALUE("Bright", &s_brightness, 0, 100, 10, "%"),
    MENU_VALUE("Blink", &s_blink_ms, 100, 2000, 100, "ms"),
    MENU_END
};

static const menu_item_t comm_menu[] = {
    MENU_VALUE("Baud", &s_rate, 1200, 115200, 1200, ""),
    MENU_TOGGLE("Sound", &s_sound),
    MENU_END
};

static const menu_item_t root_menu[] = {
    MENU_SUB("Display", display_menu),
    MENU_SUB("Comm", comm_menu),
    MENU_ACTION("Save", save),
    MENU_ACTION("Factory reset", factory),
    MENU_ACTION("About", about),
    MENU_END
};

static void draw(uint32_t row, const char *text, bool selected, void *ctx)
{
    (void)ctx;
    /* console */
    io_printf("%c %s" IO_NEWLINE, selected ? '>' : ' ', text);
    /* OLED: inverted bar on the selected line */
    if (s_oled_ok) {
        gfx_t        *g = &s_oled.gfx;
        const int16_t y = (int16_t)(12 + (row * 10U));
        gfx_fill_rect(g, 0, (int16_t)(y - 1), 128, 10, selected ? GFX_SET : GFX_CLEAR);
        gfx_set_cursor(g, 4, y);
        for (const char *p = text; *p != '\0'; p++) {
            gfx_char(g, g->cursor_x, g->cursor_y, *p, selected ? GFX_CLEAR : GFX_SET);
            g->cursor_x = (int16_t)(g->cursor_x + 6);
        }
    }
}

static void redraw(menu_t *m)
{
    io_puts("------------" IO_NEWLINE);
    if (s_oled_ok) {
        gfx_clear(&s_oled.gfx);
        gfx_set_cursor(&s_oled.gfx, 0, 0);
        gfx_puts(&s_oled.gfx, "SETTINGS");
        gfx_hline(&s_oled.gfx, 0, 9, 128, GFX_SET);
    }
    menu_render(m);
    if (s_oled_ok) {
        (void)ssd1306_flush(&s_oled);
    }
}

static void encoder_isr(hal_timer_t t, void *arg)
{
    (void)t;
    (void)arg;
    (void)encoder_update(&s_enc);
}

int main(void)
{
    menu_t m;

    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
#ifdef BOARD_PC
    sim_devices_install_default();
#endif
    (void)hal_i2c_init(HAL_I2C1, 400000U);
    s_oled_ok = (ssd1306_init(&s_oled, HAL_I2C1, SSD1306_ADDR, 64) == HAL_OK);
    encoder_init(&s_enc, ENC_A, ENC_B, 4U);
    button_init(&s_sw, ENC_SW, false, NULL);
    (void)hal_timer_start_periodic(HAL_TIMER2, 1000U, encoder_isr, NULL);

    (void)config_init();
    s_brightness = (int32_t)config_get_u32(CFG_BRIGHT, 50U);
    s_blink_ms   = (int32_t)config_get_u32(CFG_BLINK, 500U);
    s_sound      = config_get_u32(CFG_SOUND, 1U) != 0U;
    s_rate       = (int32_t)config_get_u32(CFG_RATE, 9600U);

    menu_init(&m, root_menu, MENU_ROWS, draw, NULL);
    io_println("menu: w/s move, Enter select, b back (or the encoder)");
    redraw(&m);

    for (;;) {
        bool changed = false;
        const int32_t turn = encoder_delta(&s_enc);
        for (int32_t i = 0; i < turn; i++) {
            changed |= menu_key(&m, MENU_KEY_DOWN);
        }
        for (int32_t i = 0; i > turn; i--) {
            changed |= menu_key(&m, MENU_KEY_UP);
        }
        const btn_event_t ev = button_update(&s_sw, hal_time_ms());
        if (ev == BTN_EVENT_CLICK) {
            changed |= menu_key(&m, MENU_KEY_SELECT);
        } else if (ev == BTN_EVENT_LONG_PRESS) {
            changed |= menu_key(&m, MENU_KEY_BACK);
        } else {
            /* nothing */
        }
        const int c = io_getc();
        if (c == 'w')                     { changed |= menu_key(&m, MENU_KEY_UP); }
        if (c == 's')                     { changed |= menu_key(&m, MENU_KEY_DOWN); }
        if ((c == '\r') || (c == '\n'))   { changed |= menu_key(&m, MENU_KEY_SELECT); }
        if ((c == 'b') || (c == 0x7F))    { changed |= menu_key(&m, MENU_KEY_BACK); }
        if (changed) {
            redraw(&m);
        }
        hal_sys_idle();
    }
    return 0;
}

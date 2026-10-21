/**
 * @file    gfx.c
 * @brief   Monochrome graphics (see gfx.h).
 */
#include "gfx.h"
#include "font5x7.h"
#include "fixmath.h"
#include "fmt.h"
#include "io.h"

#include <stdarg.h>
#include <stddef.h>

#define GFX_CELL_W       (6)      /* 5 px glyph + 1 px spacing */
#define GFX_CELL_H       (8)
#define GFX_PRINTF_MAX   (64U)

static int16_t abs16(int16_t v)
{
    return (int16_t)((v < 0) ? -v : v);
}

void gfx_init(gfx_t *g, uint8_t *buf, int16_t width, int16_t height)
{
    g->buf        = buf;
    g->width      = width;
    g->height     = height;
    g->cursor_x   = 0;
    g->cursor_y   = 0;
    g->text_scale = 1U;
    gfx_clear(g);
}

void gfx_fill(gfx_t *g, bool on)
{
    const int32_t n = ((int32_t)g->width * g->height) / 8;
    for (int32_t i = 0; i < n; i++) {
        g->buf[i] = on ? 0xFFU : 0x00U;
    }
    g->dirty = true;
}

void gfx_clear(gfx_t *g)
{
    gfx_fill(g, false);
    g->cursor_x = 0;
    g->cursor_y = 0;
}

void gfx_pixel(gfx_t *g, int16_t x, int16_t y, gfx_color_t c)
{
    if ((x < 0) || (y < 0) || (x >= g->width) || (y >= g->height)) {
        return;                             /* clipping */
    }
    uint8_t      *byte = &g->buf[((y / 8) * g->width) + x];
    const uint8_t mask = (uint8_t)(1U << (y & 7));
    switch (c) {
    case GFX_SET:    *byte |= mask;              break;
    case GFX_INVERT: *byte ^= mask;              break;
    case GFX_CLEAR:
    default:         *byte &= (uint8_t)~mask;    break;
    }
    g->dirty = true;
}

bool gfx_get_pixel(const gfx_t *g, int16_t x, int16_t y)
{
    if ((x < 0) || (y < 0) || (x >= g->width) || (y >= g->height)) {
        return false;
    }
    return (g->buf[((y / 8) * g->width) + x] & (1U << (y & 7))) != 0U;
}

void gfx_hline(gfx_t *g, int16_t x, int16_t y, int16_t w, gfx_color_t c)
{
    for (int16_t i = 0; i < w; i++) {
        gfx_pixel(g, (int16_t)(x + i), y, c);
    }
}

void gfx_vline(gfx_t *g, int16_t x, int16_t y, int16_t h, gfx_color_t c)
{
    for (int16_t i = 0; i < h; i++) {
        gfx_pixel(g, x, (int16_t)(y + i), c);
    }
}

void gfx_line(gfx_t *g, int16_t x0, int16_t y0, int16_t x1, int16_t y1, gfx_color_t c)
{
    /* Bresenham: integer error term, works in all octants */
    const int16_t dx  = abs16((int16_t)(x1 - x0));
    const int16_t dy  = (int16_t)-abs16((int16_t)(y1 - y0));
    const int16_t sx  = (x0 < x1) ? 1 : -1;
    const int16_t sy  = (y0 < y1) ? 1 : -1;
    int32_t       err = (int32_t)dx + dy;

    for (;;) {
        gfx_pixel(g, x0, y0, c);
        if ((x0 == x1) && (y0 == y1)) {
            break;
        }
        const int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0  = (int16_t)(x0 + sx);
        }
        if (e2 <= dx) {
            err += dx;
            y0  = (int16_t)(y0 + sy);
        }
    }
}

void gfx_rect(gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, gfx_color_t c)
{
    if ((w <= 0) || (h <= 0)) {
        return;
    }
    gfx_hline(g, x, y, w, c);
    gfx_hline(g, x, (int16_t)(y + h - 1), w, c);
    gfx_vline(g, x, (int16_t)(y + 1), (int16_t)(h - 2), c);
    gfx_vline(g, (int16_t)(x + w - 1), (int16_t)(y + 1), (int16_t)(h - 2), c);
}

void gfx_fill_rect(gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, gfx_color_t c)
{
    for (int16_t i = 0; i < h; i++) {
        gfx_hline(g, x, (int16_t)(y + i), w, c);
    }
}

void gfx_circle(gfx_t *g, int16_t cx, int16_t cy, int16_t r, gfx_color_t c)
{
    /* midpoint circle: 8-way symmetry, only additions */
    int16_t x   = r;
    int16_t y   = 0;
    int16_t err = (int16_t)(1 - r);

    while (x >= y) {
        gfx_pixel(g, (int16_t)(cx + x), (int16_t)(cy + y), c);
        gfx_pixel(g, (int16_t)(cx + y), (int16_t)(cy + x), c);
        gfx_pixel(g, (int16_t)(cx - y), (int16_t)(cy + x), c);
        gfx_pixel(g, (int16_t)(cx - x), (int16_t)(cy + y), c);
        gfx_pixel(g, (int16_t)(cx - x), (int16_t)(cy - y), c);
        gfx_pixel(g, (int16_t)(cx - y), (int16_t)(cy - x), c);
        gfx_pixel(g, (int16_t)(cx + y), (int16_t)(cy - x), c);
        gfx_pixel(g, (int16_t)(cx + x), (int16_t)(cy - y), c);
        y++;
        if (err < 0) {
            err = (int16_t)(err + (2 * y) + 1);
        } else {
            x--;
            err = (int16_t)(err + (2 * (y - x)) + 1);
        }
    }
}

void gfx_fill_circle(gfx_t *g, int16_t cx, int16_t cy, int16_t r, gfx_color_t c)
{
    for (int16_t dy = (int16_t)-r; dy <= r; dy++) {
        /* half width of the chord at this height */
        const int16_t hw = (int16_t)fx_isqrt32((uint32_t)(((int32_t)r * r) - ((int32_t)dy * dy)));
        gfx_hline(g, (int16_t)(cx - hw), (int16_t)(cy + dy), (int16_t)((2 * hw) + 1), c);
    }
}

void gfx_triangle(gfx_t *g, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, gfx_color_t c)
{
    gfx_line(g, x0, y0, x1, y1, c);
    gfx_line(g, x1, y1, x2, y2, c);
    gfx_line(g, x2, y2, x0, y0, c);
}

void gfx_bitmap(gfx_t *g, int16_t x, int16_t y, const uint8_t *bits, int16_t w, int16_t h, gfx_color_t c)
{
    const int16_t stride = (int16_t)((w + 7) / 8);
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if ((bits[(j * stride) + (i / 8)] & (0x80U >> (i & 7))) != 0U) {
                gfx_pixel(g, (int16_t)(x + i), (int16_t)(y + j), c);
            }
        }
    }
}

/* ------------------------------- text ----------------------------- */

void gfx_set_cursor(gfx_t *g, int16_t x, int16_t y)
{
    g->cursor_x = x;
    g->cursor_y = y;
}

void gfx_set_text_scale(gfx_t *g, uint8_t scale)
{
    g->text_scale = (scale == 0U) ? 1U : scale;
}

void gfx_char(gfx_t *g, int16_t x, int16_t y, char ch, gfx_color_t c)
{
    const uint8_t *glyph = font5x7_glyph(ch);
    const int16_t  s     = g->text_scale;
    for (int16_t col = 0; col < (int16_t)FONT5X7_WIDTH; col++) {
        const uint8_t bits = glyph[col];
        for (int16_t row = 0; row < (int16_t)FONT5X7_HEIGHT; row++) {
            if ((bits & (1U << row)) != 0U) {
                if (s == 1) {
                    gfx_pixel(g, (int16_t)(x + col), (int16_t)(y + row), c);
                } else {
                    gfx_fill_rect(g, (int16_t)(x + (col * s)), (int16_t)(y + (row * s)), s, s, c);
                }
            }
        }
    }
}

void gfx_putc(gfx_t *g, char ch)
{
    const int16_t cw = (int16_t)(GFX_CELL_W * g->text_scale);
    const int16_t chh = (int16_t)(GFX_CELL_H * g->text_scale);
    if (ch == '\n') {
        g->cursor_x = 0;
        g->cursor_y = (int16_t)(g->cursor_y + chh);
        return;
    }
    if ((g->cursor_x + cw) > (g->width + 1)) {      /* wrap */
        g->cursor_x = 0;
        g->cursor_y = (int16_t)(g->cursor_y + chh);
    }
    gfx_char(g, g->cursor_x, g->cursor_y, ch, GFX_SET);
    g->cursor_x = (int16_t)(g->cursor_x + cw);
}

void gfx_puts(gfx_t *g, const char *s)
{
    while (*s != '\0') {
        gfx_putc(g, *s++);
    }
}

int gfx_printf(gfx_t *g, const char *fmt, ...)
{
    char    buf[GFX_PRINTF_MAX];
    va_list args;
    va_start(args, fmt);
    const int n = fmt_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    gfx_puts(g, buf);
    return n;
}

int16_t gfx_text_width(const gfx_t *g, const char *s)
{
    int16_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return (int16_t)((n * GFX_CELL_W * g->text_scale) - ((n > 0) ? g->text_scale : 0));
}

/* ------------------------------ widgets --------------------------- */

void gfx_progress(gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t permille)
{
    if (permille > 1000U) {
        permille = 1000U;
    }
    gfx_rect(g, x, y, w, h, GFX_SET);
    const int16_t inner = (int16_t)(((uint32_t)(w - 4) * permille) / 1000U);
    gfx_fill_rect(g, (int16_t)(x + 2), (int16_t)(y + 2), inner, (int16_t)(h - 4), GFX_SET);
}

void gfx_arrow(gfx_t *g, int16_t cx, int16_t cy, int16_t len, uint16_t angle_brad)
{
    /* screen y grows downwards: up = -cos, right = +sin */
    const int32_t s  = fx_sin_q15(angle_brad);
    const int32_t co = fx_cos_q15(angle_brad);
    const int16_t tx = (int16_t)(cx + ((len * s) / 32767));
    const int16_t ty = (int16_t)(cy - ((len * co) / 32767));
    const int16_t bx = (int16_t)(cx - ((len * s) / (2 * 32767)));
    const int16_t by = (int16_t)(cy + ((len * co) / (2 * 32767)));
    /* two barbs at +-150 degrees from the tip direction */
    const uint16_t left  = (uint16_t)(angle_brad + 27307U);   /* +150 deg */
    const uint16_t right = (uint16_t)(angle_brad - 27307U);
    const int16_t  barb  = (int16_t)(len / 2);
    gfx_line(g, bx, by, tx, ty, GFX_SET);
    gfx_line(g, tx, ty, (int16_t)(tx + ((barb * fx_sin_q15(left)) / 32767)),
             (int16_t)(ty - ((barb * fx_cos_q15(left)) / 32767)), GFX_SET);
    gfx_line(g, tx, ty, (int16_t)(tx + ((barb * fx_sin_q15(right)) / 32767)),
             (int16_t)(ty - ((barb * fx_cos_q15(right)) / 32767)), GFX_SET);
}

void gfx_print_ascii(const gfx_t *g)
{
    /* each text line shows two pixel rows: upper half block, lower
     * half block, full block or space */
    io_putc('+');
    io_repeat('-', (uint32_t)g->width);
    io_puts("+" IO_NEWLINE);
    for (int16_t y = 0; y < g->height; y = (int16_t)(y + 2)) {
        io_putc('|');
        for (int16_t x = 0; x < g->width; x++) {
            const bool top = gfx_get_pixel(g, x, y);
            const bool bot = gfx_get_pixel(g, x, (int16_t)(y + 1));
            if (top && bot) {
                io_puts("\xE2\x96\x88");      /* U+2588 full block  */
            } else if (top) {
                io_puts("\xE2\x96\x80");      /* U+2580 upper half  */
            } else if (bot) {
                io_puts("\xE2\x96\x84");      /* U+2584 lower half  */
            } else {
                io_putc(' ');
            }
        }
        io_puts("|" IO_NEWLINE);
    }
    io_putc('+');
    io_repeat('-', (uint32_t)g->width);
    io_puts("+" IO_NEWLINE);
}

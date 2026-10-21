/**
 * @file    gfx.h
 * @brief   Monochrome 2D graphics on a page-organised framebuffer
 *          (SSD1306 / SH1106 / PCD8544 layout): pixels, lines
 *          (Bresenham), rectangles, circles (midpoint), text, bitmaps.
 *
 * Framebuffer layout: byte index = (y / 8) * width + x, bit = y % 8.
 * The library only draws into RAM; a display driver sends the buffer
 * (ssd1306_flush), and gfx_print_ascii() shows it in a terminal,
 * so drawings can be checked on the PC.
 */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GFX_CLEAR = 0,
    GFX_SET,
    GFX_INVERT
} gfx_color_t;

typedef struct {
    uint8_t *buf;          /**< width * height / 8 bytes            */
    int16_t  width;
    int16_t  height;
    int16_t  cursor_x;     /**< text cursor                         */
    int16_t  cursor_y;
    uint8_t  text_scale;   /**< 1 = 6x8 cell, 2 = 12x16 ...         */
    bool     dirty;
} gfx_t;

void gfx_init(gfx_t *g, uint8_t *buf, int16_t width, int16_t height);
void gfx_clear(gfx_t *g);
void gfx_fill(gfx_t *g, bool on);
void gfx_pixel(gfx_t *g, int16_t x, int16_t y, gfx_color_t c);
bool gfx_get_pixel(const gfx_t *g, int16_t x, int16_t y);
void gfx_hline(gfx_t *g, int16_t x, int16_t y, int16_t w, gfx_color_t c);
void gfx_vline(gfx_t *g, int16_t x, int16_t y, int16_t h, gfx_color_t c);
void gfx_line(gfx_t *g, int16_t x0, int16_t y0, int16_t x1, int16_t y1, gfx_color_t c);
void gfx_rect(gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, gfx_color_t c);
void gfx_fill_rect(gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, gfx_color_t c);
void gfx_circle(gfx_t *g, int16_t cx, int16_t cy, int16_t r, gfx_color_t c);
void gfx_fill_circle(gfx_t *g, int16_t cx, int16_t cy, int16_t r, gfx_color_t c);
void gfx_triangle(gfx_t *g, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, gfx_color_t c);
/** 1-bit bitmap, row-major, MSB = leftmost pixel. */
void gfx_bitmap(gfx_t *g, int16_t x, int16_t y, const uint8_t *bits, int16_t w, int16_t h, gfx_color_t c);

void gfx_set_cursor(gfx_t *g, int16_t x, int16_t y);
void gfx_set_text_scale(gfx_t *g, uint8_t scale);
void gfx_char(gfx_t *g, int16_t x, int16_t y, char ch, gfx_color_t c);
void gfx_putc(gfx_t *g, char ch);           /**< at the cursor, '\n' handled */
void gfx_puts(gfx_t *g, const char *s);
int  gfx_printf(gfx_t *g, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
/** Pixel width of a string at the current scale. */
int16_t gfx_text_width(const gfx_t *g, const char *s);

/** Horizontal progress bar with frame, 0..1000 per-mille. */
void gfx_progress(gfx_t *g, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t permille);
/** Arrow pointing to a bearing (0 = up), for navigation screens. */
void gfx_arrow(gfx_t *g, int16_t cx, int16_t cy, int16_t len, uint16_t angle_brad);

/** Draw the framebuffer with Unicode half blocks (2 pixel rows per line). */
void gfx_print_ascii(const gfx_t *g);

#endif /* GFX_H */

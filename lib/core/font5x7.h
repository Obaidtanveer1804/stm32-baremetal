/**
 * @file    font5x7.h
 * @brief   Classic 5x7 bitmap font, printable ASCII 0x20..0x7E.
 *
 * Column-major: 5 bytes per glyph, bit 0 = top row. This matches the
 * page layout of SSD1306 / PCD8544 displays, so a glyph column is
 * copied to the framebuffer in a single byte write.
 */
#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

#define FONT5X7_WIDTH   (5U)
#define FONT5X7_HEIGHT  (7U)
#define FONT5X7_FIRST   (0x20U)
#define FONT5X7_LAST    (0x7EU)

/** 5 column bytes of a character ('?' for chars outside the range). */
const uint8_t *font5x7_glyph(char c);

#endif /* FONT5X7_H */

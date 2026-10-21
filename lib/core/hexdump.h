/**
 * @file    hexdump.h
 * @brief   "hexdump -C" style memory dump through io.h.
 *
 *   08000000  00 20 00 20 c1 01 00 08  33 02 00 08 33 02 00 08  |. . ....3...3...|
 */
#ifndef HEXDUMP_H
#define HEXDUMP_H

#include <stdint.h>
#include <stddef.h>

/**
 * @param data      bytes to dump
 * @param len       number of bytes
 * @param base_addr address printed in the first column (e.g. the real
 *                  MCU address, or 0 for buffer offsets)
 */
void hexdump(const void *data, size_t len, uint32_t base_addr);

/** Dump as 32-bit words (register style), 4 per line. */
void hexdump_words(const uint32_t *words, size_t count, uint32_t base_addr);

#endif /* HEXDUMP_H */

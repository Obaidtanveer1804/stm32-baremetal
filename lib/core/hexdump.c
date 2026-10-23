/**
 * @file    hexdump.c
 * @brief   Memory dump (see hexdump.h).
 */
#include "hexdump.h"
#include "io.h"
#include "strutil.h"

#define HEXDUMP_BYTES_PER_LINE  (16U)
#define HEXDUMP_WORDS_PER_LINE  (4U)

void hexdump(const void *data, size_t len, uint32_t base_addr)
{
    const uint8_t *p = (const uint8_t *)data;

    for (size_t line = 0U; line < len; line += HEXDUMP_BYTES_PER_LINE) {
        io_print_hex(base_addr + (uint32_t)line, 8U);
        io_puts("  ");
        for (size_t i = 0U; i < HEXDUMP_BYTES_PER_LINE; i++) {
            if ((line + i) < len) {
                io_print_hex(p[line + i], 2U);
                io_putc(' ');
            } else {
                io_puts("   ");
            }
            if (i == 7U) {
                io_putc(' ');      /* gap between the two halves */
            }
        }
        io_puts(" |");
        for (size_t i = 0U; (i < HEXDUMP_BYTES_PER_LINE) && ((line + i) < len); i++) {
            const char c = (char)p[line + i];
            io_putc(chr_is_print(c) ? c : '.');
        }
        io_puts("|" IO_NEWLINE);
    }
}

void hexdump_words(const uint32_t *words, size_t count, uint32_t base_addr)
{
    for (size_t i = 0U; i < count; i++) {
        if ((i % HEXDUMP_WORDS_PER_LINE) == 0U) {
            if (i != 0U) {
                io_puts(IO_NEWLINE);
            }
            io_print_hex(base_addr + (uint32_t)(i * 4U), 8U);
            io_puts(": ");
        }
        io_print_hex(words[i], 8U);
        io_putc(' ');
    }
    io_puts(IO_NEWLINE);
}

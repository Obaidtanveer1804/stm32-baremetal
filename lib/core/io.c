/**
 * @file    io.c
 * @brief   Console I/O through function pointers (see io.h).
 */
#include "io.h"
#include "fmt.h"

#include <stdarg.h>

#define IO_NUM_BUF_SIZE  (48U)

static io_putc_fn s_putc;
static io_getc_fn s_getc;

void io_init(io_putc_fn putc_fn, io_getc_fn getc_fn)
{
    s_putc = putc_fn;
    s_getc = getc_fn;
}

void io_putc(char ch)
{
    if (s_putc != NULL) {
        s_putc(ch);
    }
}

void io_puts(const char *s)
{
    while (*s != '\0') {
        io_putc(*s++);
    }
}

void io_println(const char *s)
{
    io_puts(s);
    io_puts(IO_NEWLINE);
}

void io_write(const void *data, size_t len)
{
    const char *p = (const char *)data;
    while (len-- > 0U) {
        io_putc(*p++);
    }
}

static void io_fmt_sink(char ch, void *ctx)
{
    (void)ctx;
    io_putc(ch);
}

int io_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int n = fmt_vformat(io_fmt_sink, NULL, fmt, args);
    va_end(args);
    return n;
}

int io_getc(void)
{
    return (s_getc != NULL) ? s_getc() : IO_EOF;
}

char io_getc_blocking(void)
{
    int c;
    do {
        c = io_getc();
    } while (c == IO_EOF);
    return (c == IO_CLOSED) ? (char)IO_CHAR_EOT : (char)c;
}

void io_print_dec(int32_t value)
{
    char buf[IO_NUM_BUF_SIZE];
    (void)fmt_i32(buf, sizeof(buf), value);
    io_puts(buf);
}

void io_print_udec(uint32_t value)
{
    char buf[IO_NUM_BUF_SIZE];
    (void)fmt_u32(buf, sizeof(buf), value, 10U, 0U);
    io_puts(buf);
}

void io_print_hex(uint32_t value, uint32_t digits)
{
    char buf[IO_NUM_BUF_SIZE];
    (void)fmt_hex(buf, sizeof(buf), value, digits);
    io_puts(buf);
}

void io_print_bin(uint32_t value, uint32_t bits)
{
    char buf[IO_NUM_BUF_SIZE];
    (void)fmt_bin(buf, sizeof(buf), value, bits, false);
    io_puts(buf);
}

void io_print_fixed(int32_t raw, uint32_t scale, uint32_t decimals)
{
    char buf[IO_NUM_BUF_SIZE];
    (void)fmt_fixed(buf, sizeof(buf), raw, scale, decimals);
    io_puts(buf);
}

void io_repeat(char ch, uint32_t count)
{
    while (count-- > 0U) {
        io_putc(ch);
    }
}

void io_ansi_clear_screen(void)
{
    io_puts("\x1b[2J\x1b[H");
}

void io_ansi_clear_line(void)
{
    io_puts("\r\x1b[K");
}

void io_ansi_cursor(uint32_t row, uint32_t col)
{
    io_printf("\x1b[%u;%uH", (unsigned)row, (unsigned)col);
}

void io_ansi_color(uint32_t color)
{
    io_printf("\x1b[%um", (unsigned)color);
}

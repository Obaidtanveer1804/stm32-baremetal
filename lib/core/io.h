/**
 * @file    io.h
 * @brief   Console I/O decoupled from the hardware.
 *
 * Application code prints through io_*(). Which device gets the
 * characters is decided once at start-up with io_init():
 *
 *     io_init(hal_uart_putc_console, hal_uart_getc_console);
 *
 * On the PC port the same functions end up on stdout/stdin, so any
 * logic that only uses io_*() runs unchanged on both targets
 * (exactly the idea of the binary/hex counters in tp2.c).
 */
#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef void (*io_putc_fn)(char ch);
typedef int  (*io_getc_fn)(void);     /**< byte, IO_EOF (nothing yet) or IO_CLOSED */

#define IO_EOF          (-1)     /**< no character available right now   */
#define IO_CLOSED       (-2)     /**< input closed for good (PC: end of a piped script) */
#define IO_CHAR_EOT     (0x04)   /**< returned by io_getc_blocking on IO_CLOSED */
#define IO_NEWLINE      "\r\n"

void io_init(io_putc_fn putc_fn, io_getc_fn getc_fn);

void io_putc(char ch);
void io_puts(const char *s);            /**< no newline appended          */
void io_println(const char *s);         /**< appends "\r\n"               */
void io_write(const void *data, size_t len);
int  io_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/** Non-blocking read: a byte (0..255), IO_EOF or IO_CLOSED. */
int  io_getc(void);
/** Blocking read (spins on io_getc). */
char io_getc_blocking(void);

/* Convenience printers built on fmt.h */
void io_print_dec(int32_t value);
void io_print_udec(uint32_t value);
void io_print_hex(uint32_t value, uint32_t digits);
void io_print_bin(uint32_t value, uint32_t bits);
void io_print_fixed(int32_t raw, uint32_t scale, uint32_t decimals);
void io_repeat(char ch, uint32_t count);

/** ANSI terminal helpers (picocom / minicom / screen understand them). */
void io_ansi_clear_screen(void);
void io_ansi_clear_line(void);
void io_ansi_cursor(uint32_t row, uint32_t col);
void io_ansi_color(uint32_t color);     /**< 30..37 fg, 0 = reset */

#define IO_COLOR_RESET   (0U)
#define IO_COLOR_RED     (31U)
#define IO_COLOR_GREEN   (32U)
#define IO_COLOR_YELLOW  (33U)
#define IO_COLOR_BLUE    (34U)
#define IO_COLOR_MAGENTA (35U)
#define IO_COLOR_CYAN    (36U)
#define IO_COLOR_WHITE   (37U)

#endif /* IO_H */

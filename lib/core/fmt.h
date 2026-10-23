/**
 * @file    fmt.h
 * @brief   Tiny printf engine and number-to-text conversions.
 *
 * No malloc, no floats, no libc printf (saves several kB of flash).
 * The engine writes through a callback, so the same code serves
 * UART output (io.c), string buffers (fmt_snprintf) or anything else.
 *
 * Supported: %d %i %u %x %X %o %b %c %s %p %%
 * Flags    : '-' (left), '0' (zero pad), '+', ' ' (space), '#' (0x/0b)
 * Width    : number or '*'.  Precision: '.N' (strings: max chars,
 *            integers: min digits).
 * Length   : 'l' (long), 'll' (long long), 'h', 'hh' (accepted, ints).
 */
#ifndef FMT_H
#define FMT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** Character sink: called once per output character. */
typedef void (*fmt_out_fn)(char ch, void *ctx);

/** Format into a sink; returns the number of characters produced. */
int  fmt_vformat(fmt_out_fn out, void *ctx, const char *fmt, va_list args);
int  fmt_format(fmt_out_fn out, void *ctx, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));

/** snprintf replacement: always NUL-terminates when size > 0. */
int  fmt_vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int  fmt_snprintf(char *buf, size_t size, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));

/* -------- plain conversions (no format string parsing) ------------- */

/**
 * @brief  Convert an unsigned value to text in any base (2..16).
 * @param  min_digits pad with '0' up to this many digits (0 = none).
 * @return number of characters written (without the NUL), 0 on error.
 */
size_t fmt_u32(char *buf, size_t size, uint32_t value, uint32_t base,
               uint32_t min_digits);
size_t fmt_i32(char *buf, size_t size, int32_t value);

/** Fixed number of hex digits, e.g. fmt_hex(buf, 0xA5, 4) -> "00A5". */
size_t fmt_hex(char *buf, size_t size, uint32_t value, uint32_t digits);

/** Binary with @p bits digits, optional '_' every 4 bits ("1010_0101"). */
size_t fmt_bin(char *buf, size_t size, uint32_t value, uint32_t bits,
               bool group_nibbles);

/**
 * @brief  Fixed-point value as decimal, no float needed.
 *         fmt_fixed(buf, n, 12345, 100, 2) -> "123.45"
 * @param  scale    the value is (raw / scale)
 * @param  decimals digits after the point
 */
size_t fmt_fixed(char *buf, size_t size, int32_t raw, uint32_t scale,
                 uint32_t decimals);

#endif /* FMT_H */

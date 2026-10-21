/**
 * @file    strutil.h
 * @brief   String helpers written without libc (string.h / stdlib.h).
 *
 * Every function that writes takes the destination size, so none of
 * them can overflow a buffer. Results are always NUL-terminated.
 */
#ifndef STRUTIL_H
#define STRUTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

size_t str_len(const char *s);
size_t str_nlen(const char *s, size_t max_len);
int    str_cmp(const char *a, const char *b);
int    str_ncmp(const char *a, const char *b, size_t n);
int    str_icmp(const char *a, const char *b);          /**< case-insensitive */
bool   str_eq(const char *a, const char *b);
bool   str_ieq(const char *a, const char *b);
bool   str_starts_with(const char *s, const char *prefix);
bool   str_ends_with(const char *s, const char *suffix);

/** Copy with truncation (like strlcpy); returns strlen(src). */
size_t str_copy(char *dst, const char *src, size_t dst_size);
/** Append with truncation (like strlcat); returns the wanted length. */
size_t str_cat(char *dst, const char *src, size_t dst_size);

const char *str_chr(const char *s, char c);
const char *str_rchr(const char *s, char c);
const char *str_str(const char *haystack, const char *needle);

char  *str_to_upper(char *s);
char  *str_to_lower(char *s);
/** Strip leading/trailing whitespace in place; returns the new start. */
char  *str_trim(char *s);
/** Reverse in place. */
char  *str_reverse(char *s);
/** Number of occurrences of @p c. */
size_t str_count_char(const char *s, char c);

/** Character classes (ASCII only, locale independent). */
static inline bool chr_is_digit(char c)  { return (c >= '0') && (c <= '9'); }
static inline bool chr_is_upper(char c)  { return (c >= 'A') && (c <= 'Z'); }
static inline bool chr_is_lower(char c)  { return (c >= 'a') && (c <= 'z'); }
static inline bool chr_is_alpha(char c)  { return chr_is_upper(c) || chr_is_lower(c); }
static inline bool chr_is_alnum(char c)  { return chr_is_alpha(c) || chr_is_digit(c); }
static inline bool chr_is_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f') || (c == '\v');
}
static inline bool chr_is_xdigit(char c)
{
    return chr_is_digit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
}
static inline bool chr_is_print(char c)  { return (c >= 0x20) && (c < 0x7F); }
/** Toggle case with a single XOR: 'a' ^ 0x20 == 'A' (letters only). */
static inline char chr_to_upper(char c)  { return chr_is_lower(c) ? (char)(c ^ 0x20) : c; }
static inline char chr_to_lower(char c)  { return chr_is_upper(c) ? (char)(c ^ 0x20) : c; }

/**
 * @brief  Split @p s in place into whitespace-separated tokens.
 *         Double quotes group words: say "hello world" -> 2 tokens.
 * @return number of tokens stored in argv (<= max_tokens).
 */
uint32_t str_tokenize(char *s, char *argv[], uint32_t max_tokens);

/**
 * @brief  Split in place on one delimiter char ("a,b,,c" -> 4 fields).
 * @return number of fields.
 */
uint32_t str_split(char *s, char delim, char *fields[], uint32_t max_fields);

/* --------------------------- number parsing ---------------------- */

/**
 * @brief  Parse an unsigned integer. Accepts decimal, 0x hex, 0b binary,
 *         0o octal and '_' separators (0b1010_0101). Whole string must
 *         be consumed, overflow is detected.
 * @return true on success.
 */
bool str_to_u32(const char *s, uint32_t *out);
/** Same, with an optional leading '+' or '-'. */
bool str_to_i32(const char *s, int32_t *out);
/** Parse "12.34" into a fixed-point integer with @p scale (e.g. 100). */
bool str_to_fixed(const char *s, uint32_t scale, int32_t *out);
/** Parse "on/off/1/0/true/false/yes/no/high/low". */
bool str_to_bool(const char *s, bool *out);
/** Value of one hex digit, or -1. */
int  chr_hex_value(char c);
/**
 * @brief  Parse a hex byte string ("DEADBEEF" or "de ad be ef").
 * @return number of bytes written to @p out.
 */
size_t str_hex_to_bytes(const char *s, uint8_t *out, size_t out_size);

#endif /* STRUTIL_H */

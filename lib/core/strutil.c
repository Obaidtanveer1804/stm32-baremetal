/**
 * @file    strutil.c
 * @brief   String helpers (see strutil.h).
 */
#include "strutil.h"

size_t str_len(const char *s)
{
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}

size_t str_nlen(const char *s, size_t max_len)
{
    size_t n = 0U;
    while ((n < max_len) && (s[n] != '\0')) {
        n++;
    }
    return n;
}

int str_cmp(const char *a, const char *b)
{
    while ((*a != '\0') && (*a == *b)) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int str_ncmp(const char *a, const char *b, size_t n)
{
    while (n > 0U) {
        if ((*a != *b) || (*a == '\0')) {
            return (int)(unsigned char)*a - (int)(unsigned char)*b;
        }
        a++;
        b++;
        n--;
    }
    return 0;
}

int str_icmp(const char *a, const char *b)
{
    char ca;
    char cb;
    do {
        ca = chr_to_lower(*a++);
        cb = chr_to_lower(*b++);
    } while ((ca != '\0') && (ca == cb));
    return (int)(unsigned char)ca - (int)(unsigned char)cb;
}

bool str_eq(const char *a, const char *b)
{
    return str_cmp(a, b) == 0;
}

bool str_ieq(const char *a, const char *b)
{
    return str_icmp(a, b) == 0;
}

bool str_starts_with(const char *s, const char *prefix)
{
    while (*prefix != '\0') {
        if (*s++ != *prefix++) {
            return false;
        }
    }
    return true;
}

bool str_ends_with(const char *s, const char *suffix)
{
    const size_t ls = str_len(s);
    const size_t lx = str_len(suffix);
    return (lx <= ls) && str_eq(&s[ls - lx], suffix);
}

size_t str_copy(char *dst, const char *src, size_t dst_size)
{
    const size_t src_len = str_len(src);
    if (dst_size > 0U) {
        const size_t n = (src_len < (dst_size - 1U)) ? src_len : (dst_size - 1U);
        for (size_t i = 0U; i < n; i++) {
            dst[i] = src[i];
        }
        dst[n] = '\0';
    }
    return src_len;
}

size_t str_cat(char *dst, const char *src, size_t dst_size)
{
    const size_t dst_len = str_nlen(dst, dst_size);
    if (dst_len == dst_size) {
        return dst_size + str_len(src);     /* dst was not terminated */
    }
    return dst_len + str_copy(&dst[dst_len], src, dst_size - dst_len);
}

const char *str_chr(const char *s, char c)
{
    for (;; s++) {
        if (*s == c) {
            return s;
        }
        if (*s == '\0') {
            return NULL;
        }
    }
}

const char *str_rchr(const char *s, char c)
{
    const char *last = NULL;
    for (;; s++) {
        if (*s == c) {
            last = s;
        }
        if (*s == '\0') {
            return last;
        }
    }
}

const char *str_str(const char *haystack, const char *needle)
{
    const size_t n = str_len(needle);
    if (n == 0U) {
        return haystack;
    }
    for (; *haystack != '\0'; haystack++) {
        if (str_ncmp(haystack, needle, n) == 0) {
            return haystack;
        }
    }
    return NULL;
}

char *str_to_upper(char *s)
{
    for (char *p = s; *p != '\0'; p++) {
        *p = chr_to_upper(*p);
    }
    return s;
}

char *str_to_lower(char *s)
{
    for (char *p = s; *p != '\0'; p++) {
        *p = chr_to_lower(*p);
    }
    return s;
}

char *str_trim(char *s)
{
    char *end;

    while (chr_is_space(*s)) {
        s++;
    }
    end = s + str_len(s);
    while ((end > s) && chr_is_space(end[-1])) {
        end--;
    }
    *end = '\0';
    return s;
}

char *str_reverse(char *s)
{
    size_t len = str_len(s);
    for (size_t i = 0U; i < (len / 2U); i++) {
        const char tmp   = s[i];
        s[i]             = s[len - 1U - i];
        s[len - 1U - i]  = tmp;
    }
    return s;
}

size_t str_count_char(const char *s, char c)
{
    size_t n = 0U;
    for (; *s != '\0'; s++) {
        if (*s == c) {
            n++;
        }
    }
    return n;
}

uint32_t str_tokenize(char *s, char *argv[], uint32_t max_tokens)
{
    uint32_t argc = 0U;

    while ((*s != '\0') && (argc < max_tokens)) {
        while (chr_is_space(*s)) {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        if (*s == '"') {
            /* quoted token: everything up to the closing quote */
            s++;
            argv[argc++] = s;
            while ((*s != '\0') && (*s != '"')) {
                s++;
            }
        } else {
            argv[argc++] = s;
            while ((*s != '\0') && !chr_is_space(*s)) {
                s++;
            }
        }
        if (*s != '\0') {
            *s++ = '\0';
        }
    }
    return argc;
}

uint32_t str_split(char *s, char delim, char *fields[], uint32_t max_fields)
{
    uint32_t n = 0U;

    if (max_fields == 0U) {
        return 0U;
    }
    fields[n++] = s;
    for (; *s != '\0'; s++) {
        if (*s == delim) {
            *s = '\0';
            if (n >= max_fields) {
                break;
            }
            fields[n++] = s + 1;
        }
    }
    return n;
}

int chr_hex_value(char c)
{
    if (chr_is_digit(c)) {
        return c - '0';
    }
    c = chr_to_lower(c);
    if ((c >= 'a') && (c <= 'f')) {
        return (c - 'a') + 10;
    }
    return -1;
}

bool str_to_u32(const char *s, uint32_t *out)
{
    uint32_t base   = 10U;
    uint32_t value  = 0U;
    bool     digits = false;

    if ((s == NULL) || (*s == '\0')) {
        return false;
    }
    if (s[0] == '0') {
        const char p = chr_to_lower(s[1]);
        if (p == 'x')      { base = 16U; s += 2; }
        else if (p == 'b') { base = 2U;  s += 2; }
        else if (p == 'o') { base = 8U;  s += 2; }
        else               { /* plain decimal (leading zero allowed) */ }
    }
    for (; *s != '\0'; s++) {
        if (*s == '_') {
            continue;
        }
        const int d = chr_hex_value(*s);
        if ((d < 0) || ((uint32_t)d >= base)) {
            return false;
        }
        /* overflow check: value * base + d must fit in 32 bits */
        if (value > ((0xFFFFFFFFUL - (uint32_t)d) / base)) {
            return false;
        }
        value  = (value * base) + (uint32_t)d;
        digits = true;
    }
    if (!digits) {
        return false;
    }
    *out = value;
    return true;
}

bool str_to_i32(const char *s, int32_t *out)
{
    bool     negative = false;
    uint32_t mag;

    if (s == NULL) {
        return false;
    }
    if ((*s == '-') || (*s == '+')) {
        negative = (*s == '-');
        s++;
    }
    if (!str_to_u32(s, &mag)) {
        return false;
    }
    if (negative) {
        if (mag > 0x80000000UL) {
            return false;
        }
        *out = (int32_t)(0U - mag);
    } else {
        if (mag > 0x7FFFFFFFUL) {
            return false;
        }
        *out = (int32_t)mag;
    }
    return true;
}

bool str_to_fixed(const char *s, uint32_t scale, int32_t *out)
{
    bool     negative  = false;
    uint64_t int_part  = 0U;
    uint64_t frac_num  = 0U;
    uint64_t frac_den  = 1U;
    bool     digits    = false;

    if ((s == NULL) || (scale == 0U)) {
        return false;
    }
    if ((*s == '-') || (*s == '+')) {
        negative = (*s == '-');
        s++;
    }
    for (; chr_is_digit(*s); s++) {
        int_part = (int_part * 10U) + (uint64_t)(*s - '0');
        digits   = true;
        if (int_part > 0x7FFFFFFFULL) {
            return false;
        }
    }
    if (*s == '.') {
        s++;
        for (; chr_is_digit(*s); s++) {
            if (frac_den < 1000000000ULL) {     /* ignore extra digits */
                frac_num = (frac_num * 10U) + (uint64_t)(*s - '0');
                frac_den *= 10U;
            }
            digits = true;
        }
    }
    if ((*s != '\0') || !digits) {
        return false;
    }
    const uint64_t raw = (int_part * scale) + (((frac_num * scale) + (frac_den / 2U)) / frac_den);
    if (raw > 0x7FFFFFFFULL) {
        return false;
    }
    *out = negative ? -(int32_t)raw : (int32_t)raw;
    return true;
}

bool str_to_bool(const char *s, bool *out)
{
    static const char *const true_words[]  = { "1", "on",  "true",  "yes", "high", "set" };
    static const char *const false_words[] = { "0", "off", "false", "no",  "low",  "clr" };

    for (size_t i = 0U; i < (sizeof(true_words) / sizeof(true_words[0])); i++) {
        if (str_ieq(s, true_words[i])) {
            *out = true;
            return true;
        }
        if (str_ieq(s, false_words[i])) {
            *out = false;
            return true;
        }
    }
    return false;
}

size_t str_hex_to_bytes(const char *s, uint8_t *out, size_t out_size)
{
    size_t n      = 0U;
    int    high   = -1;

    if ((s[0] == '0') && ((s[1] == 'x') || (s[1] == 'X'))) {
        s += 2;
    }
    for (; (*s != '\0') && (n < out_size); s++) {
        if (chr_is_space(*s) || (*s == ':') || (*s == '_')) {
            continue;
        }
        const int d = chr_hex_value(*s);
        if (d < 0) {
            break;
        }
        if (high < 0) {
            high = d;
        } else {
            out[n++] = (uint8_t)((high << 4) | d);
            high     = -1;
        }
    }
    return n;
}

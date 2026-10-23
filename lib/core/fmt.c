/**
 * @file    fmt.c
 * @brief   Tiny printf engine (see fmt.h).
 */
#include "fmt.h"

#define FMT_FLAG_LEFT     (1U << 0)
#define FMT_FLAG_ZERO     (1U << 1)
#define FMT_FLAG_PLUS     (1U << 2)
#define FMT_FLAG_SPACE    (1U << 3)
#define FMT_FLAG_ALT      (1U << 4)
#define FMT_FLAG_UPPER    (1U << 5)
#define FMT_FLAG_PREC     (1U << 6)

#define FMT_NUM_BUF_SIZE  (66U)   /* 64 binary digits + sign + NUL */

static const char digits_lower[] = "0123456789abcdef";
static const char digits_upper[] = "0123456789ABCDEF";

/* Output state shared by the helpers below. */
typedef struct {
    fmt_out_fn out;
    void      *ctx;
    int        count;
} fmt_state_t;

static void emit(fmt_state_t *st, char ch)
{
    st->out(ch, st->ctx);
    st->count++;
}

static void emit_repeat(fmt_state_t *st, char ch, int n)
{
    while (n-- > 0) {
        emit(st, ch);
    }
}

/* Write digits of value (reversed) into tmp; returns digit count. */
static int u64_to_digits(char *tmp, uint64_t value, uint32_t base, bool upper)
{
    const char *table = upper ? digits_upper : digits_lower;
    int n = 0;
    do {
        tmp[n++] = table[value % base];
        value /= base;
    } while (value != 0U);
    return n;
}

static void emit_integer(fmt_state_t *st, uint64_t magnitude, bool negative,
                         uint32_t base, uint32_t flags, int width, int prec)
{
    char tmp[FMT_NUM_BUF_SIZE];
    char prefix[3];
    int  prefix_len = 0;
    int  ndigits;
    int  nzeros;
    int  total;

    if (((flags & FMT_FLAG_PREC) != 0U) && (prec == 0) && (magnitude == 0U)) {
        ndigits = 0;    /* printf("%.0d", 0) prints nothing */
    } else {
        ndigits = u64_to_digits(tmp, magnitude, base, (flags & FMT_FLAG_UPPER) != 0U);
    }

    if (negative) {
        prefix[prefix_len++] = '-';
    } else if ((flags & FMT_FLAG_PLUS) != 0U) {
        prefix[prefix_len++] = '+';
    } else if ((flags & FMT_FLAG_SPACE) != 0U) {
        prefix[prefix_len++] = ' ';
    } else {
        /* no sign */
    }

    if (((flags & FMT_FLAG_ALT) != 0U) && (magnitude != 0U)) {
        if (base == 16U) {
            prefix[prefix_len++] = '0';
            prefix[prefix_len++] = ((flags & FMT_FLAG_UPPER) != 0U) ? 'X' : 'x';
        } else if (base == 2U) {
            prefix[prefix_len++] = '0';
            prefix[prefix_len++] = 'b';
        } else if (base == 8U) {
            prefix[prefix_len++] = '0';
        } else {
            /* no prefix for decimal */
        }
    }

    nzeros = ((flags & FMT_FLAG_PREC) != 0U) && (prec > ndigits) ? (prec - ndigits) : 0;
    total  = prefix_len + nzeros + ndigits;

    /* '0' flag pads with zeros after the sign, ignored with '-' or precision */
    if (((flags & FMT_FLAG_ZERO) != 0U) && ((flags & (FMT_FLAG_LEFT | FMT_FLAG_PREC)) == 0U)
        && (width > total)) {
        nzeros += width - total;
        total   = width;
    }

    if (((flags & FMT_FLAG_LEFT) == 0U) && (width > total)) {
        emit_repeat(st, ' ', width - total);
    }
    for (int i = 0; i < prefix_len; i++) {
        emit(st, prefix[i]);
    }
    emit_repeat(st, '0', nzeros);
    while (ndigits > 0) {
        emit(st, tmp[--ndigits]);
    }
    if (((flags & FMT_FLAG_LEFT) != 0U) && (width > total)) {
        emit_repeat(st, ' ', width - total);
    }
}

static void emit_string(fmt_state_t *st, const char *s, uint32_t flags,
                        int width, int prec)
{
    int len = 0;

    if (s == NULL) {
        s = "(null)";
    }
    /* strnlen limited by precision */
    while ((s[len] != '\0') && (((flags & FMT_FLAG_PREC) == 0U) || (len < prec))) {
        len++;
    }
    if (((flags & FMT_FLAG_LEFT) == 0U) && (width > len)) {
        emit_repeat(st, ' ', width - len);
    }
    for (int i = 0; i < len; i++) {
        emit(st, s[i]);
    }
    if (((flags & FMT_FLAG_LEFT) != 0U) && (width > len)) {
        emit_repeat(st, ' ', width - len);
    }
}

static int parse_number(const char **p)
{
    int n = 0;
    while ((**p >= '0') && (**p <= '9')) {
        n = (n * 10) + (**p - '0');
        (*p)++;
    }
    return n;
}

int fmt_vformat(fmt_out_fn out, void *ctx, const char *fmt, va_list args)
{
    fmt_state_t st = { out, ctx, 0 };
    const char *p  = fmt;

    while (*p != '\0') {
        uint32_t flags  = 0U;
        int      width  = 0;
        int      prec   = 0;
        int      length = 0;   /* 0:int 1:long 2:long long */

        if (*p != '%') {
            emit(&st, *p++);
            continue;
        }
        p++;

        /* flags */
        for (;;) {
            if (*p == '-')      { flags |= FMT_FLAG_LEFT;  }
            else if (*p == '0') { flags |= FMT_FLAG_ZERO;  }
            else if (*p == '+') { flags |= FMT_FLAG_PLUS;  }
            else if (*p == ' ') { flags |= FMT_FLAG_SPACE; }
            else if (*p == '#') { flags |= FMT_FLAG_ALT;   }
            else                { break; }
            p++;
        }

        /* width */
        if (*p == '*') {
            width = va_arg(args, int);
            if (width < 0) {
                flags |= FMT_FLAG_LEFT;
                width  = -width;
            }
            p++;
        } else {
            width = parse_number(&p);
        }

        /* precision */
        if (*p == '.') {
            p++;
            flags |= FMT_FLAG_PREC;
            if (*p == '*') {
                prec = va_arg(args, int);
                p++;
            } else {
                prec = parse_number(&p);
            }
            if (prec < 0) {
                flags &= ~FMT_FLAG_PREC;
                prec   = 0;
            }
        }

        /* length modifiers */
        while ((*p == 'l') || (*p == 'h') || (*p == 'z')) {
            if (*p == 'l') {
                length++;
            } else if (*p == 'z') {
                length = (sizeof(size_t) > sizeof(long)) ? 2 : 1;
            } else {
                /* 'h' / 'hh': promoted to int anyway */
            }
            p++;
        }

        switch (*p) {
        case 'd':
        case 'i': {
            int64_t v;
            if (length >= 2)      { v = va_arg(args, long long); }
            else if (length == 1) { v = va_arg(args, long); }
            else                  { v = va_arg(args, int); }
            /* avoid overflow on INT64_MIN: negate in unsigned domain */
            const uint64_t mag = (v < 0) ? (0U - (uint64_t)v) : (uint64_t)v;
            emit_integer(&st, mag, v < 0, 10U, flags, width, prec);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o':
        case 'b': {
            uint64_t v;
            uint32_t base = 10U;
            if (length >= 2)      { v = va_arg(args, unsigned long long); }
            else if (length == 1) { v = va_arg(args, unsigned long); }
            else                  { v = va_arg(args, unsigned int); }
            if ((*p == 'x') || (*p == 'X')) { base = 16U; }
            if (*p == 'X')                  { flags |= FMT_FLAG_UPPER; }
            if (*p == 'o')                  { base = 8U; }
            if (*p == 'b')                  { base = 2U; }
            emit_integer(&st, v, false, base, flags & ~(FMT_FLAG_PLUS | FMT_FLAG_SPACE),
                         width, prec);
            break;
        }
        case 'p': {
            const uintptr_t v = (uintptr_t)va_arg(args, void *);
            emit_integer(&st, (uint64_t)v, false, 16U, flags | FMT_FLAG_ALT, width, prec);
            break;
        }
        case 'c': {
            const char c[2] = { (char)va_arg(args, int), '\0' };
            /* a NUL char must still be emitted, so handle it apart */
            if (c[0] == '\0') {
                emit(&st, '\0');
            } else {
                emit_string(&st, c, flags & ~FMT_FLAG_PREC, width, 0);
            }
            break;
        }
        case 's':
            emit_string(&st, va_arg(args, const char *), flags, width, prec);
            break;
        case '%':
            emit(&st, '%');
            break;
        case '\0':
            /* trailing '%': print it and stop */
            emit(&st, '%');
            return st.count;
        default:
            /* unknown conversion: print verbatim */
            emit(&st, '%');
            emit(&st, *p);
            break;
        }
        p++;
    }
    return st.count;
}

int fmt_format(fmt_out_fn out, void *ctx, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int n = fmt_vformat(out, ctx, fmt, args);
    va_end(args);
    return n;
}

/* -------------------- buffer sink for snprintf -------------------- */

typedef struct {
    char  *buf;
    size_t size;
    size_t pos;
} buf_sink_t;

static void buf_sink_out(char ch, void *ctx)
{
    buf_sink_t *s = (buf_sink_t *)ctx;
    if ((s->pos + 1U) < s->size) {
        s->buf[s->pos] = ch;
    }
    s->pos++;
}

int fmt_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    buf_sink_t sink = { buf, size, 0U };
    const int n = fmt_vformat(buf_sink_out, &sink, fmt, args);
    if (size > 0U) {
        buf[(sink.pos < size) ? sink.pos : (size - 1U)] = '\0';
    }
    return n;
}

int fmt_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const int n = fmt_vsnprintf(buf, size, fmt, args);
    va_end(args);
    return n;
}

/* ---------------------- direct conversions ------------------------ */

size_t fmt_u32(char *buf, size_t size, uint32_t value, uint32_t base,
               uint32_t min_digits)
{
    char   tmp[33];
    size_t n = 0U;
    size_t i = 0U;

    if ((buf == NULL) || (base < 2U) || (base > 16U)) {
        return 0U;
    }
    do {
        tmp[n++] = digits_upper[value % base];
        value   /= base;
    } while ((value != 0U) && (n < sizeof(tmp)));
    while ((n < min_digits) && (n < sizeof(tmp))) {
        tmp[n++] = '0';
    }
    if ((n + 1U) > size) {
        if (size > 0U) {
            buf[0] = '\0';
        }
        return 0U;
    }
    while (n > 0U) {
        buf[i++] = tmp[--n];
    }
    buf[i] = '\0';
    return i;
}

size_t fmt_i32(char *buf, size_t size, int32_t value)
{
    if (value >= 0) {
        return fmt_u32(buf, size, (uint32_t)value, 10U, 0U);
    }
    if (size < 3U) {
        if (size > 0U) {
            buf[0] = '\0';
        }
        return 0U;
    }
    buf[0] = '-';
    const size_t n = fmt_u32(&buf[1], size - 1U, 0U - (uint32_t)value, 10U, 0U);
    return (n == 0U) ? 0U : (n + 1U);
}

size_t fmt_hex(char *buf, size_t size, uint32_t value, uint32_t digits)
{
    if (digits < 8U) {
        value &= ((uint32_t)1U << (digits * 4U)) - 1U;
    }
    return fmt_u32(buf, size, value, 16U, digits);
}

size_t fmt_bin(char *buf, size_t size, uint32_t value, uint32_t bits,
               bool group_nibbles)
{
    size_t i = 0U;

    if ((bits == 0U) || (bits > 32U)) {
        return 0U;
    }
    for (uint32_t b = bits; b > 0U; b--) {
        if ((i + 2U) > size) {
            break;
        }
        buf[i++] = ((value >> (b - 1U)) & 1U) ? '1' : '0';
        /* separator between nibbles, not after the last digit */
        if (group_nibbles && (b > 1U) && (((b - 1U) % 4U) == 0U) && ((i + 2U) <= size)) {
            buf[i++] = '_';
        }
    }
    if (size > 0U) {
        buf[i] = '\0';
    }
    return i;
}

size_t fmt_fixed(char *buf, size_t size, int32_t raw, uint32_t scale,
                 uint32_t decimals)
{
    const bool     negative = raw < 0;
    const uint32_t mag      = negative ? (0U - (uint32_t)raw) : (uint32_t)raw;
    uint32_t       pow10    = 1U;
    size_t         i        = 0U;

    if ((scale == 0U) || (decimals > 9U)) {
        return 0U;
    }
    for (uint32_t k = 0U; k < decimals; k++) {
        pow10 *= 10U;
    }
    uint32_t       int_part  = mag / scale;
    /* rounded fractional part, computed in 64 bit to avoid overflow */
    uint64_t       frac      = (((uint64_t)(mag % scale) * pow10) + (scale / 2U)) / scale;
    if (frac >= pow10) {    /* rounding carried into the integer part */
        frac -= pow10;
        int_part++;
    }
    if (negative && ((int_part != 0U) || (frac != 0U))) {
        if (size < 2U) {
            return 0U;
        }
        buf[i++] = '-';
    }
    const size_t n = fmt_u32(&buf[i], size - i, int_part, 10U, 0U);
    if (n == 0U) {
        return 0U;
    }
    i += n;
    if (decimals > 0U) {
        if ((i + 1U + decimals + 1U) > size) {
            return 0U;
        }
        buf[i++] = '.';
        i += fmt_u32(&buf[i], size - i, (uint32_t)frac, 10U, decimals);
    }
    return i;
}

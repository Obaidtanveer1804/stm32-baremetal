/**
 * @file    counter.c
 * @brief   Generic counters (see counter.h).
 */
#include "counter.h"
#include "codes.h"
#include "io.h"

void counter_init(counter_t *c, int32_t min, int32_t max, int32_t step,
                  counter_mode_t mode)
{
    if (min > max) {
        const int32_t tmp = min;
        min = max;
        max = tmp;
    }
    c->min   = min;
    c->max   = max;
    c->step  = (step == 0) ? 1 : step;
    c->mode  = mode;
    c->wraps = 0U;
    c->value = (c->step > 0) ? min : max;
}

bool counter_step(counter_t *c)
{
    /* compute in 64 bit so value + step can never overflow */
    const int64_t next  = (int64_t)c->value + c->step;
    const int64_t range = (int64_t)c->max - c->min + 1;
    bool          event = false;

    if ((next > c->max) || (next < c->min)) {
        event = true;
        c->wraps++;
        switch (c->mode) {
        case COUNTER_MODE_SATURATE:
            c->value = (next > c->max) ? c->max : c->min;
            break;
        case COUNTER_MODE_BOUNCE:
            c->step  = -c->step;
            c->value = (next > c->max) ? c->max : c->min;
            break;
        case COUNTER_MODE_WRAP:
        default: {
            /* modular wrap keeps the phase for steps > 1 */
            int64_t offset = (next - c->min) % range;
            if (offset < 0) {
                offset += range;
            }
            c->value = (int32_t)(c->min + offset);
            break;
        }
        }
    } else {
        c->value = (int32_t)next;
    }
    return event;
}

void counter_set(counter_t *c, int32_t value)
{
    if (value < c->min) {
        value = c->min;
    }
    if (value > c->max) {
        value = c->max;
    }
    c->value = value;
}

void counter_reset(counter_t *c)
{
    c->value = (c->step > 0) ? c->min : c->max;
    c->wraps = 0U;
}

int32_t counter_get(const counter_t *c)
{
    return c->value;
}

/* --------------------------- display ------------------------------ */

void counter_print_decimal(uint32_t value, uint32_t digits)
{
    io_puts("decimal count: ");
    io_printf("%0*u", (int)digits, (unsigned)value);
    io_puts(IO_NEWLINE);
}

void counter_print_binary(uint32_t value, uint32_t bits)
{
    io_puts("binary counter: ");
    io_print_bin(value, bits);
    io_puts(IO_NEWLINE);
}

void counter_print_hex(uint32_t value, uint32_t digits)
{
    io_puts("hex counter : 0x");
    io_print_hex(value, digits);
    io_puts(IO_NEWLINE);
}

void counter_print_bcd(uint32_t value, uint32_t digits)
{
    const uint32_t bcd = codes_bin_to_bcd(value);
    io_puts("bcd counter : ");
    for (uint32_t d = digits; d > 0U; d--) {
        io_print_bin((bcd >> ((d - 1U) * 4U)) & 0xFU, 4U);
        if (d > 1U) {
            io_putc(' ');
        }
    }
    io_puts(IO_NEWLINE);
}

void counter_print_gray(uint32_t value, uint32_t bits)
{
    io_puts("gray counter: ");
    io_print_bin(codes_bin_to_gray(value), bits);
    io_puts(IO_NEWLINE);
}

void counter_print_all(uint32_t value, uint32_t bits)
{
    const uint32_t hex_digits = (bits + 3U) / 4U;
    io_printf("dec %5u | hex 0x", (unsigned)value);
    io_print_hex(value, hex_digits);
    io_puts(" | bin ");
    io_print_bin(value, bits);
    io_puts(" | gray ");
    io_print_bin(codes_bin_to_gray(value), bits);
    io_puts(IO_NEWLINE);
}

/* ------------------------- 16 -> 32 bit --------------------------- */

void counter_ext16_init(counter_ext16_t *e, uint16_t hw_now)
{
    e->last     = hw_now;
    e->extended = hw_now;
}

uint32_t counter_ext16_update(counter_ext16_t *e, uint16_t hw_now)
{
    /* modular difference: correct across one hardware overflow */
    const uint16_t delta = (uint16_t)(hw_now - e->last);
    e->last      = hw_now;
    e->extended += delta;
    return e->extended;
}

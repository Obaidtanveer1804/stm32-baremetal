/**
 * @file    stats.c
 * @brief   Running statistics and histogram (see stats.h).
 */
#include "stats.h"
#include "fixmath.h"
#include "io.h"

#include <stddef.h>

void stats_reset(stats_t *s)
{
    s->count   = 0U;
    s->min     = INT32_MAX;
    s->max     = INT32_MIN;
    s->sum     = 0;
    s->mean_q8 = 0;
    s->m2_q16  = 0;
}

void stats_add(stats_t *s, int32_t sample)
{
    const int64_t x_q8 = (int64_t)sample * 256;

    s->count++;
    s->sum += sample;
    if (sample < s->min) {
        s->min = sample;
    }
    if (sample > s->max) {
        s->max = sample;
    }
    /* Welford: numerically stable single pass mean / variance */
    const int64_t delta = x_q8 - s->mean_q8;
    s->mean_q8 += delta / (int64_t)s->count;
    const int64_t delta2 = x_q8 - s->mean_q8;
    s->m2_q16 += delta * delta2;
}

int32_t stats_mean(const stats_t *s)
{
    if (s->count == 0U) {
        return 0;
    }
    const int64_t c = s->count;
    return (int32_t)((s->sum >= 0) ? ((s->sum + (c / 2)) / c) : ((s->sum - (c / 2)) / c));
}

uint32_t stats_variance(const stats_t *s)
{
    if (s->count < 2U) {
        return 0U;
    }
    return (uint32_t)((s->m2_q16 / (int64_t)s->count) >> 16);
}

uint32_t stats_stddev(const stats_t *s)
{
    if (s->count < 2U) {
        return 0U;
    }
    /* sqrt(m2 / n) with m2 in Q16 -> result in Q8, then round */
    const uint32_t sd_q8 = fx_isqrt64((uint64_t)(s->m2_q16 / (int64_t)s->count));
    return (sd_q8 + 128U) >> 8;
}

uint32_t stats_stddev_x100(const stats_t *s)
{
    if (s->count < 2U) {
        return 0U;
    }
    const uint32_t sd_q8 = fx_isqrt64((uint64_t)(s->m2_q16 / (int64_t)s->count));
    return (uint32_t)((((uint64_t)sd_q8 * 100U) + 128U) >> 8);
}

void stats_print(const stats_t *s, const char *label)
{
    if (label != NULL) {
        io_puts(label);
        io_puts(": ");
    }
    if (s->count == 0U) {
        io_println("no samples");
        return;
    }
    io_printf("n=%lu min=%ld max=%ld mean=%ld sd=",
              (unsigned long)s->count, (long)s->min, (long)s->max, (long)stats_mean(s));
    io_print_fixed((int32_t)stats_stddev_x100(s), 100U, 2U);
    io_puts(IO_NEWLINE);
}

/* --------------------------- histogram ---------------------------- */

void histo_init(histo_t *h, uint32_t *bins, uint16_t nbins, int32_t lo, int32_t hi)
{
    h->bins  = bins;
    h->nbins = (nbins == 0U) ? 1U : nbins;
    h->lo    = lo;
    h->width = (hi > lo) ? (((hi - lo) + h->nbins - 1) / h->nbins) : 1;
    h->under = 0U;
    h->over  = 0U;
    for (uint16_t i = 0U; i < h->nbins; i++) {
        bins[i] = 0U;
    }
}

void histo_add(histo_t *h, int32_t sample)
{
    if (sample < h->lo) {
        h->under++;
        return;
    }
    const uint32_t idx = (uint32_t)(sample - h->lo) / (uint32_t)h->width;
    if (idx >= h->nbins) {
        h->over++;
    } else {
        h->bins[idx]++;
    }
}

void histo_print(const histo_t *h, uint32_t bar_width)
{
    uint32_t peak = 1U;
    for (uint16_t i = 0U; i < h->nbins; i++) {
        if (h->bins[i] > peak) {
            peak = h->bins[i];
        }
    }
    for (uint16_t i = 0U; i < h->nbins; i++) {
        const int32_t  from = h->lo + ((int32_t)i * h->width);
        const uint32_t len  = (uint32_t)(((uint64_t)h->bins[i] * bar_width) / peak);
        io_printf("%7ld | ", (long)from);
        io_repeat('#', len);
        io_printf(" %lu" IO_NEWLINE, (unsigned long)h->bins[i]);
    }
    if ((h->under != 0U) || (h->over != 0U)) {
        io_printf("under=%lu over=%lu" IO_NEWLINE,
                  (unsigned long)h->under, (unsigned long)h->over);
    }
}

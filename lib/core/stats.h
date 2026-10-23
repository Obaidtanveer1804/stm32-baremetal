/**
 * @file    stats.h
 * @brief   Running statistics on integer samples: min, max, mean,
 *          variance (Welford, fixed point), plus a histogram.
 *
 * Useful to characterise ADC noise, loop jitter, interrupt latency...
 */
#ifndef STATS_H
#define STATS_H

#include <stdint.h>

typedef struct {
    uint32_t count;
    int32_t  min;
    int32_t  max;
    int64_t  sum;
    int64_t  mean_q8;    /**< running mean * 256 (Welford)       */
    int64_t  m2_q16;     /**< sum of squared deviations * 65536  */
} stats_t;

void     stats_reset(stats_t *s);
void     stats_add(stats_t *s, int32_t sample);
int32_t  stats_mean(const stats_t *s);          /**< rounded       */
/** Population variance, in sample units squared. */
uint32_t stats_variance(const stats_t *s);
uint32_t stats_stddev(const stats_t *s);
/** Standard deviation * 100, to print 2 decimals with fmt_fixed. */
uint32_t stats_stddev_x100(const stats_t *s);
/** Print "n=.. min=.. max=.. mean=.. sd=.." via io.h */
void     stats_print(const stats_t *s, const char *label);

/* --------------------------- histogram ---------------------------- */
typedef struct {
    uint32_t *bins;
    uint16_t  nbins;
    int32_t   lo;
    int32_t   width;     /**< value range per bin                  */
    uint32_t  under;
    uint32_t  over;
} histo_t;

void histo_init(histo_t *h, uint32_t *bins, uint16_t nbins, int32_t lo, int32_t hi);
void histo_add(histo_t *h, int32_t sample);
/** ASCII bar chart, @p bar_width characters for the largest bin. */
void histo_print(const histo_t *h, uint32_t bar_width);

#endif /* STATS_H */

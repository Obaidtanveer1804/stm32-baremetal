/**
 * @file    filter.c
 * @brief   Integer digital filters (see filter.h).
 */
#include "filter.h"

#define EMA_FRAC_BITS   (8U)
#define BIQUAD_Q        (14U)
#define FIR_Q           (15U)

/* ----------------------- moving average --------------------------- */

void filt_mavg_init(filt_mavg_t *f, int32_t *window, uint16_t size)
{
    f->window = window;
    f->size   = (size == 0U) ? 1U : size;
    f->index  = 0U;
    f->count  = 0U;
    f->sum    = 0;
    for (uint16_t i = 0U; i < f->size; i++) {
        window[i] = 0;
    }
}

int32_t filt_mavg_update(filt_mavg_t *f, int32_t sample)
{
    /* subtract the sample leaving the window, add the new one */
    f->sum += sample - f->window[f->index];
    f->window[f->index] = sample;
    f->index++;
    if (f->index >= f->size) {
        f->index = 0U;
    }
    if (f->count < f->size) {
        f->count++;
    }
    return f->sum / (int32_t)f->count;
}

/* ------------------- exponential moving average ------------------- */

void filt_ema_init(filt_ema_t *f, uint8_t shift)
{
    f->state  = 0;
    f->shift  = shift;
    f->primed = false;
}

int32_t filt_ema_update(filt_ema_t *f, int32_t sample)
{
    const int32_t x = sample * ((int32_t)1 << EMA_FRAC_BITS);
    if (!f->primed) {
        f->state  = x;       /* start at the first sample, no ramp-up */
        f->primed = true;
    } else {
        f->state += (x - f->state) >> f->shift;
    }
    /* round back to integer units */
    return (f->state + ((int32_t)1 << (EMA_FRAC_BITS - 1U))) >> EMA_FRAC_BITS;
}

/* ---------------------------- median ------------------------------ */

int32_t filt_median3(int32_t a, int32_t b, int32_t c)
{
    if (a > b) {
        const int32_t t = a;
        a = b;
        b = t;
    }
    /* now a <= b: median is b clamped to [a, c] ordering */
    if (b > c) {
        b = (a > c) ? a : c;
    }
    return b;
}

void filt_median_init(filt_median_t *f, uint8_t size)
{
    if (size > FILT_MEDIAN_MAX) {
        size = FILT_MEDIAN_MAX;
    }
    if ((size & 1U) == 0U) {
        size--;                         /* force an odd window */
    }
    f->size  = (size == 0U) ? 1U : size;
    f->index = 0U;
    f->count = 0U;
}

int32_t filt_median_update(filt_median_t *f, int32_t sample)
{
    int32_t sorted[FILT_MEDIAN_MAX];

    f->window[f->index] = sample;
    f->index = (uint8_t)((f->index + 1U) % f->size);
    if (f->count < f->size) {
        f->count++;
    }
    /* insertion sort of a copy: tiny N, cache friendly, no recursion */
    for (uint8_t i = 0U; i < f->count; i++) {
        const int32_t v = f->window[i];
        int32_t       j = (int32_t)i - 1;
        while ((j >= 0) && (sorted[j] > v)) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = v;
    }
    return sorted[f->count / 2U];
}

/* -------------------------- IIR biquad ---------------------------- */

void filt_biquad_init(filt_biquad_t *f, int16_t b0, int16_t b1, int16_t b2,
                      int16_t a1, int16_t a2)
{
    f->b0 = b0;
    f->b1 = b1;
    f->b2 = b2;
    f->a1 = a1;
    f->a2 = a2;
    f->x1 = 0;
    f->x2 = 0;
    f->y1 = 0;
    f->y2 = 0;
}

int32_t filt_biquad_update(filt_biquad_t *f, int32_t sample)
{
    int64_t acc = (int64_t)f->b0 * sample
                + (int64_t)f->b1 * f->x1
                + (int64_t)f->b2 * f->x2
                - (int64_t)f->a1 * f->y1
                - (int64_t)f->a2 * f->y2;
    const int32_t y = (int32_t)((acc + ((int64_t)1 << (BIQUAD_Q - 1U))) >> BIQUAD_Q);

    f->x2 = f->x1;
    f->x1 = sample;
    f->y2 = f->y1;
    f->y1 = y;
    return y;
}

/* ------------------------------ FIR ------------------------------- */

void filt_fir_init(filt_fir_t *f, const int16_t *taps, int32_t *history,
                   uint16_t ntaps)
{
    f->taps    = taps;
    f->history = history;
    f->ntaps   = ntaps;
    f->index   = 0U;
    for (uint16_t i = 0U; i < ntaps; i++) {
        history[i] = 0;
    }
}

int32_t filt_fir_update(filt_fir_t *f, int32_t sample)
{
    int64_t  acc = 0;
    uint16_t h   = f->index;

    f->history[f->index] = sample;
    /* newest sample pairs with taps[0], walking the history backwards */
    for (uint16_t t = 0U; t < f->ntaps; t++) {
        acc += (int64_t)f->taps[t] * f->history[h];
        h = (h == 0U) ? (uint16_t)(f->ntaps - 1U) : (uint16_t)(h - 1U);
    }
    f->index++;
    if (f->index >= f->ntaps) {
        f->index = 0U;
    }
    return (int32_t)((acc + ((int64_t)1 << (FIR_Q - 1U))) >> FIR_Q);
}

/* --------------------- non-linear helpers ------------------------- */

void filt_hyst_init(filt_hyst_t *f, int32_t low, int32_t high, bool initial)
{
    f->low   = low;
    f->high  = high;
    f->state = initial;
}

bool filt_hyst_update(filt_hyst_t *f, int32_t sample)
{
    if (sample >= f->high) {
        f->state = true;
    } else if (sample <= f->low) {
        f->state = false;
    } else {
        /* inside the band: keep the previous state */
    }
    return f->state;
}

void filt_slew_init(filt_slew_t *f, int32_t initial, int32_t max_step)
{
    f->value    = initial;
    f->max_step = (max_step < 0) ? -max_step : max_step;
}

int32_t filt_slew_update(filt_slew_t *f, int32_t target)
{
    const int32_t delta = target - f->value;
    if (delta > f->max_step) {
        f->value += f->max_step;
    } else if (delta < -f->max_step) {
        f->value -= f->max_step;
    } else {
        f->value = target;
    }
    return f->value;
}

int32_t filt_deadband(int32_t x, int32_t center, int32_t band)
{
    const int32_t d = x - center;
    return ((d <= band) && (d >= -band)) ? center : x;
}

void filt_peak_init(filt_peak_t *f, uint8_t decay_shift)
{
    f->peak        = 0;
    f->decay_shift = decay_shift;
}

int32_t filt_peak_update(filt_peak_t *f, int32_t sample)
{
    if (sample >= f->peak) {
        f->peak = sample;
    } else {
        /* decay towards the sample by a fraction of the gap (>= 1) */
        int32_t step = (f->peak - sample) >> f->decay_shift;
        if (step == 0) {
            step = 1;
        }
        f->peak -= step;
    }
    return f->peak;
}

int32_t filt_oversample(const int32_t *samples, uint32_t extra_bits)
{
    const uint32_t count = (uint32_t)1U << (2U * extra_bits);
    int64_t        sum   = 0;
    for (uint32_t i = 0U; i < count; i++) {
        sum += samples[i];
    }
    return (int32_t)(sum >> extra_bits);
}

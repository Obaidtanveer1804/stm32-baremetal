/**
 * @file    dsp.c
 * @brief   Fixed-point DSP (see dsp.h).
 */
#include "dsp.h"
#include "fixmath.h"
#include "bits.h"

/* ------------------------------- FFT ------------------------------ */

/* (a * b) >> 15 with rounding, Q15 multiply */
static int32_t mul_q15(int32_t a, int32_t b)
{
    return ((a * b) + (1 << 14)) >> 15;
}

bool dsp_fft(dsp_cplx_t *x, uint32_t log2n)
{
    if ((log2n == 0U) || (log2n > DSP_FFT_MAX_LOG2)) {
        return false;
    }
    const uint32_t n = 1U << log2n;

    /* 1. bit-reversal permutation: x[i] <-> x[rev(i)] */
    for (uint32_t i = 0U; i < n; i++) {
        const uint32_t j = bits_reverse32(i) >> (32U - log2n);
        if (j > i) {
            const dsp_cplx_t t = x[i];
            x[i] = x[j];
            x[j] = t;
        }
    }
    /* 2. butterflies, decimation in time */
    for (uint32_t stage = 1U; stage <= log2n; stage++) {
        const uint32_t m    = 1U << stage;          /* butterfly span     */
        const uint32_t half = m >> 1;
        const uint32_t step = 65536U >> stage;      /* twiddle angle step, brad */
        for (uint32_t k = 0U; k < half; k++) {
            /* W = exp(-j 2 pi k / m) = cos - j sin */
            const uint16_t angle = (uint16_t)(k * step);
            const int32_t  wr    = fx_cos_q15(angle);
            const int32_t  wi    = -(int32_t)fx_sin_q15(angle);
            for (uint32_t i = k; i < n; i += m) {
                dsp_cplx_t *a = &x[i];
                dsp_cplx_t *b = &x[i + half];
                const int32_t tr = mul_q15(b->re, wr) - mul_q15(b->im, wi);
                const int32_t ti = mul_q15(b->re, wi) + mul_q15(b->im, wr);
                /* scale by 1/2 at every stage: no overflow possible */
                const int32_t ar = a->re;
                const int32_t ai = a->im;
                a->re = (int16_t)((ar + tr) >> 1);
                a->im = (int16_t)((ai + ti) >> 1);
                b->re = (int16_t)((ar - tr) >> 1);
                b->im = (int16_t)((ai - ti) >> 1);
            }
        }
    }
    return true;
}

bool dsp_fft_magnitude(const int16_t *in, uint32_t log2n, dsp_cplx_t *work, uint16_t *mag)
{
    if ((log2n == 0U) || (log2n > DSP_FFT_MAX_LOG2)) {
        return false;
    }
    const uint32_t n = 1U << log2n;
    for (uint32_t i = 0U; i < n; i++) {
        work[i].re = in[i];
        work[i].im = 0;
    }
    (void)dsp_fft(work, log2n);
    /* X/n out of the FFT: a real tone of amplitude A shows A/2 in bin k
     * and A/2 in bin n-k, so amplitude = 2 |X/n| (DC: |X/n|) */
    for (uint32_t k = 0U; k <= (n / 2U); k++) {
        const uint32_t m = fx_hypot(work[k].re, work[k].im);
        const uint32_t a = ((k == 0U) || (k == (n / 2U))) ? m : (2U * m);
        mag[k] = (uint16_t)((a > 0xFFFFU) ? 0xFFFFU : a);
    }
    return true;
}

void dsp_window_hann(int16_t *x, uint32_t n)
{
    if (n < 2U) {
        return;
    }
    for (uint32_t i = 0U; i < n; i++) {
        /* w = 0.5 - 0.5 cos(2 pi i / (n - 1)) in Q15 */
        const uint16_t angle = (uint16_t)((i * 65536U) / (n - 1U));
        const int32_t  w     = (32767 - fx_cos_q15(angle)) / 2;
        x[i] = (int16_t)mul_q15(x[i], w);
    }
}

uint32_t dsp_peak(const uint16_t *mag, uint32_t from, uint32_t to, uint16_t *value)
{
    uint32_t best = from;
    uint16_t v    = 0U;
    for (uint32_t k = from; k < to; k++) {
        if (mag[k] > v) {
            v    = mag[k];
            best = k;
        }
    }
    if (value != NULL) {
        *value = v;
    }
    return best;
}

uint32_t dsp_bin_freq_c(uint32_t bin, uint32_t sample_hz, uint32_t n)
{
    return (n == 0U) ? 0U : (uint32_t)(((uint64_t)bin * sample_hz * 100U) / n);
}

/* ----------------------------- Goertzel --------------------------- */

uint32_t dsp_goertzel(const int16_t *x, uint32_t n, uint32_t target_hz, uint32_t sample_hz)
{
    if ((n == 0U) || (sample_hz == 0U)) {
        return 0U;
    }
    /* nearest bin k; cos(2 pi k / n) in Q15, the recurrence uses 2 cos */
    const uint32_t k      = (uint32_t)((((uint64_t)n * target_hz) + (sample_hz / 2U)) / sample_hz);
    const uint16_t angle  = (uint16_t)((k * 65536U) / n);
    const int64_t  coeff  = fx_cos_q15(angle);
    int64_t        s1     = 0;
    int64_t        s2     = 0;
    for (uint32_t i = 0U; i < n; i++) {
        /* s = x + 2 cos(w) s1 - s2 */
        const int64_t s = x[i] + ((2 * coeff * s1) >> 15) - s2;
        s2 = s1;
        s1 = s;
    }
    /* power = s1^2 + s2^2 - 2 cos(w) s1 s2, normalised by n^2 / 4 so a
     * tone of amplitude A gives about A^2 */
    const int64_t p = (s1 * s1) + (s2 * s2) - (((2 * coeff * s1) >> 15) * s2);
    const int64_t norm = ((int64_t)n * n) / 4;
    const int64_t r = (p < 0) ? 0 : (p / ((norm == 0) ? 1 : norm));
    return (uint32_t)((r > 0xFFFFFFFFLL) ? 0xFFFFFFFFLL : r);
}

/* ---------------------------- statistics -------------------------- */

int32_t dsp_mean(const int16_t *x, uint32_t n)
{
    int64_t sum = 0;
    for (uint32_t i = 0U; i < n; i++) {
        sum += x[i];
    }
    return (n == 0U) ? 0 : (int32_t)(sum / (int64_t)n);
}

void dsp_remove_dc(int16_t *x, uint32_t n)
{
    const int32_t m = dsp_mean(x, n);
    for (uint32_t i = 0U; i < n; i++) {
        const int32_t v = x[i] - m;
        x[i] = (int16_t)((v > INT16_MAX) ? INT16_MAX : ((v < INT16_MIN) ? INT16_MIN : v));
    }
}

uint32_t dsp_rms(const int16_t *x, uint32_t n)
{
    uint64_t sum = 0U;
    if (n == 0U) {
        return 0U;
    }
    for (uint32_t i = 0U; i < n; i++) {
        sum += (uint64_t)((int32_t)x[i] * x[i]);
    }
    const uint64_t ms = (sum + (n / 2U)) / n;
    uint32_t       r  = fx_isqrt64(ms);
    if ((ms - ((uint64_t)r * r)) > r) {
        r++;
    }
    return r;
}

uint32_t dsp_zero_cross_freq_c(const int16_t *x, uint32_t n, uint32_t sample_hz)
{
    /* rising crossings with linear interpolation of the exact instant,
     * in 1/256 sample units, for sub-sample precision */
    int64_t  first   = -1;
    int64_t  last    = -1;
    uint32_t crosses = 0U;
    for (uint32_t i = 1U; i < n; i++) {
        if ((x[i - 1U] < 0) && (x[i] >= 0)) {
            const int32_t dy  = x[i] - x[i - 1U];
            const int64_t pos = ((int64_t)(i - 1U) * 256) + ((dy == 0) ? 0 : (((int64_t)-x[i - 1U] * 256) / dy));
            if (first < 0) {
                first = pos;
            }
            last = pos;
            crosses++;
        }
    }
    if ((crosses < 2U) || (last <= first)) {
        return 0U;
    }
    /* (crosses - 1) periods in (last - first) / 256 samples */
    return (uint32_t)(((uint64_t)(crosses - 1U) * sample_hz * 100U * 256U) / (uint64_t)(last - first));
}

void dsp_minmax(const int16_t *x, uint32_t n, int16_t *min, int16_t *max)
{
    int16_t lo = INT16_MAX;
    int16_t hi = INT16_MIN;
    for (uint32_t i = 0U; i < n; i++) {
        lo = (x[i] < lo) ? x[i] : lo;
        hi = (x[i] > hi) ? x[i] : hi;
    }
    *min = lo;
    *max = hi;
}

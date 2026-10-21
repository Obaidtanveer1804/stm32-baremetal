/**
 * @file    filter.h
 * @brief   Integer digital filters for sensor / ADC data.
 *
 * Every filter is a small struct + init + update(sample) -> output.
 * No floats, no malloc: buffers are supplied by the caller.
 */
#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

/* ----------------------- moving average --------------------------- */
/** Boxcar average over N samples with a running sum: O(1) per sample. */
typedef struct {
    int32_t *window;
    uint16_t size;
    uint16_t index;
    uint16_t count;      /**< samples received, saturates at size */
    int32_t  sum;
} filt_mavg_t;

void    filt_mavg_init(filt_mavg_t *f, int32_t *window, uint16_t size);
int32_t filt_mavg_update(filt_mavg_t *f, int32_t sample);

/* ------------------- exponential moving average ------------------- */
/**
 * y += (x - y) / 2^shift. alpha = 1/2^shift, no multiply, no divide.
 * The state keeps 8 extra fractional bits so small steps are not lost.
 */
typedef struct {
    int32_t  state;      /**< y * 256 */
    uint8_t  shift;
    bool     primed;
} filt_ema_t;

void    filt_ema_init(filt_ema_t *f, uint8_t shift);
int32_t filt_ema_update(filt_ema_t *f, int32_t sample);

/* ---------------------------- median ------------------------------ */
int32_t filt_median3(int32_t a, int32_t b, int32_t c);

/** Running median over an odd window (<= FILT_MEDIAN_MAX). */
#define FILT_MEDIAN_MAX  (15U)
typedef struct {
    int32_t  window[FILT_MEDIAN_MAX];
    uint8_t  size;
    uint8_t  index;
    uint8_t  count;
} filt_median_t;

void    filt_median_init(filt_median_t *f, uint8_t size);
int32_t filt_median_update(filt_median_t *f, int32_t sample);

/* -------------------------- IIR biquad ---------------------------- */
/**
 * Direct form I, coefficients in Q14 (1.0 = 16384), a0 normalised to 1:
 *   y = b0 x + b1 x1 + b2 x2 - a1 y1 - a2 y2
 * Design coefficients on the PC (e.g. scipy) then multiply by 16384.
 */
typedef struct {
    int16_t b0, b1, b2, a1, a2;
    int32_t x1, x2, y1, y2;
} filt_biquad_t;

void    filt_biquad_init(filt_biquad_t *f, int16_t b0, int16_t b1, int16_t b2,
                         int16_t a1, int16_t a2);
int32_t filt_biquad_update(filt_biquad_t *f, int32_t sample);

/* ------------------------------ FIR ------------------------------- */
/** Generic FIR with Q15 taps, circular history buffer. */
typedef struct {
    const int16_t *taps;
    int32_t       *history;
    uint16_t       ntaps;
    uint16_t       index;
} filt_fir_t;

void    filt_fir_init(filt_fir_t *f, const int16_t *taps, int32_t *history,
                      uint16_t ntaps);
int32_t filt_fir_update(filt_fir_t *f, int32_t sample);

/* --------------------- non-linear helpers ------------------------- */
/** Schmitt trigger: true above @p high, false below @p low, else keep. */
typedef struct {
    int32_t low;
    int32_t high;
    bool    state;
} filt_hyst_t;

void filt_hyst_init(filt_hyst_t *f, int32_t low, int32_t high, bool initial);
bool filt_hyst_update(filt_hyst_t *f, int32_t sample);

/** Slew-rate limiter: output moves at most @p max_step per call. */
typedef struct {
    int32_t value;
    int32_t max_step;
} filt_slew_t;

void    filt_slew_init(filt_slew_t *f, int32_t initial, int32_t max_step);
int32_t filt_slew_update(filt_slew_t *f, int32_t target);

/** Dead band: values within +-band of @p center return @p center. */
int32_t filt_deadband(int32_t x, int32_t center, int32_t band);

/** Peak hold with exponential decay (VU-meter style). */
typedef struct {
    int32_t peak;
    uint8_t decay_shift;
} filt_peak_t;

void    filt_peak_init(filt_peak_t *f, uint8_t decay_shift);
int32_t filt_peak_update(filt_peak_t *f, int32_t sample);

/**
 * Oversampling and decimation: sums 4^extra_bits samples and shifts
 * right by extra_bits, gaining extra_bits of resolution (needs noise).
 * E.g. 16 samples of a 12-bit ADC -> one 14-bit result.
 */
int32_t filt_oversample(const int32_t *samples, uint32_t extra_bits);

#endif /* FILTER_H */

/**
 * @file    dsp.h
 * @brief   Fixed-point signal analysis for ADC data: radix-2 FFT (Q15),
 *          Goertzel single-tone detector, RMS, mean/DC removal,
 *          zero-crossing frequency, peak finding, windowing.
 *
 * Everything is integer: on a Cortex-M3 without FPU a 128-point FFT
 * takes ~1 ms at 24 MHz. Samples are int16 (e.g. ADC raw - 2048).
 */
#ifndef DSP_H
#define DSP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DSP_FFT_MAX_LOG2  (9U)      /* up to 512 points */

typedef struct {
    int16_t re;
    int16_t im;
} dsp_cplx_t;

/**
 * In-place complex FFT, n = 2^log2n points, Q15 data. Every stage
 * scales by 1/2 so nothing overflows: the result is X[k] / n.
 */
bool     dsp_fft(dsp_cplx_t *x, uint32_t log2n);
/** Real input -> magnitude of bins 0..n/2 (|X| * 2 / n, i.e. amplitudes). */
bool     dsp_fft_magnitude(const int16_t *in, uint32_t log2n, dsp_cplx_t *work, uint16_t *mag);
/** Hann window in place (reduces spectral leakage). */
void     dsp_window_hann(int16_t *x, uint32_t n);
/** Index of the largest bin in [from, to), and its value. */
uint32_t dsp_peak(const uint16_t *mag, uint32_t from, uint32_t to, uint16_t *value);
/** Bin index -> frequency in centi-hertz. */
uint32_t dsp_bin_freq_c(uint32_t bin, uint32_t sample_hz, uint32_t n);

/** Goertzel power at one frequency (DTMF, tone detection): O(n), no table. */
uint32_t dsp_goertzel(const int16_t *x, uint32_t n, uint32_t target_hz, uint32_t sample_hz);

int32_t  dsp_mean(const int16_t *x, uint32_t n);
void     dsp_remove_dc(int16_t *x, uint32_t n);
/** RMS of the samples (after DC removal if you want AC RMS). */
uint32_t dsp_rms(const int16_t *x, uint32_t n);
/** Frequency from rising zero crossings, centi-hertz (0 if < 2 crossings). */
uint32_t dsp_zero_cross_freq_c(const int16_t *x, uint32_t n, uint32_t sample_hz);
/** Min and max sample. */
void     dsp_minmax(const int16_t *x, uint32_t n, int16_t *min, int16_t *max);

#endif /* DSP_H */

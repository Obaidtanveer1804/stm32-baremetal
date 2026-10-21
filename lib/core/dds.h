/**
 * @file    dds.h
 * @brief   Direct Digital Synthesis: a 32-bit phase accumulator turns a
 *          fixed sample rate into any output frequency with 0.01 Hz
 *          resolution (what the DDS chips like AD9833 do in hardware).
 *
 *   f_out = tuning_word * f_sample / 2^32
 *
 * Call dds_next() from a timer interrupt at f_sample and write the
 * result to the DAC or a PWM duty. Waveforms are computed from the
 * top phase bits: sine (table), triangle, saw, square (with duty).
 */
#ifndef DDS_H
#define DDS_H

#include <stdint.h>

typedef enum {
    DDS_SINE = 0,
    DDS_TRIANGLE,
    DDS_SAW,
    DDS_SQUARE
} dds_wave_t;

typedef struct {
    uint32_t   phase;
    uint32_t   tuning;          /**< phase increment per sample          */
    uint32_t   sample_hz;
    dds_wave_t wave;
    uint32_t   duty_threshold;  /**< square: phase below it is "high"   */
    uint16_t   amplitude;       /**< output peak, 0..max                */
    uint16_t   offset;          /**< added to the output                */
} dds_t;

void     dds_init(dds_t *d, uint32_t sample_hz, dds_wave_t wave, uint16_t amplitude, uint16_t offset);
/** Frequency in centi-hertz (44000 = 440.00 Hz). */
void     dds_set_freq_c(dds_t *d, uint32_t freq_centihz);
void     dds_set_duty(dds_t *d, uint32_t permille);
void     dds_set_phase(dds_t *d, uint16_t brad);
/** Next sample: offset + amplitude * wave(phase), every wave in [-1, 1],
 *  clamped to 0..65535. */
uint16_t dds_next(dds_t *d);
/** Tuning word for a frequency (portable, unit-tested). */
uint32_t dds_tuning_word(uint32_t freq_centihz, uint32_t sample_hz);

#endif /* DDS_H */

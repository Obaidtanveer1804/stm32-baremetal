/**
 * @file    dds.c
 * @brief   Phase-accumulator synthesis (see dds.h).
 */
#include "dds.h"
#include "fixmath.h"

uint32_t dds_tuning_word(uint32_t freq_centihz, uint32_t sample_hz)
{
    if (sample_hz == 0U) {
        return 0U;
    }
    /* 2^32 * f / fs, with f in 0.01 Hz: (f << 32) / (fs * 100), rounded */
    const uint64_t den = (uint64_t)sample_hz * 100U;
    return (uint32_t)((((uint64_t)freq_centihz << 32) + (den / 2U)) / den);
}

void dds_init(dds_t *d, uint32_t sample_hz, dds_wave_t wave, uint16_t amplitude, uint16_t offset)
{
    d->phase          = 0U;
    d->tuning         = 0U;
    d->sample_hz      = sample_hz;
    d->wave           = wave;
    d->duty_threshold = 0x80000000U;
    d->amplitude      = amplitude;
    d->offset         = offset;
}

void dds_set_freq_c(dds_t *d, uint32_t freq_centihz)
{
    d->tuning = dds_tuning_word(freq_centihz, d->sample_hz);
}

void dds_set_duty(dds_t *d, uint32_t permille)
{
    if (permille > 1000U) {
        permille = 1000U;
    }
    d->duty_threshold = (uint32_t)(((uint64_t)permille << 32) / 1000U);
}

void dds_set_phase(dds_t *d, uint16_t brad)
{
    d->phase = (uint32_t)brad << 16;
}

uint16_t dds_next(dds_t *d)
{
    const uint32_t ph = d->phase;
    int32_t        v;          /* Q15 in [-32767, 32767] or [0, 32767] */

    d->phase += d->tuning;     /* wraps modulo 2^32: that IS the period */
    switch (d->wave) {
    case DDS_TRIANGLE: {
        /* fold the saw: 0..1..0 */
        const uint32_t t = (ph & 0x80000000U) ? ~ph : ph;
        v = (int32_t)((t >> 15) & 0xFFFFU) - 32768;   /* -32768..32767 */
        break;
    }
    case DDS_SAW:
        v = (int32_t)(ph >> 16) - 32768;              /* -32768..32767 */
        break;
    case DDS_SQUARE:
        v = (ph < d->duty_threshold) ? 32767 : -32767;
        break;
    case DDS_SINE:
    default:
        v = fx_sin_q15((uint16_t)(ph >> 16));
        break;
    }
    const int32_t out = (int32_t)d->offset + (((int32_t)d->amplitude * v) / 32767);
    return (uint16_t)((out < 0) ? 0 : ((out > 0xFFFF) ? 0xFFFF : out));
}

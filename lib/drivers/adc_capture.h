/**
 * @file    adc_capture.h
 * @brief   Timer-paced ADC acquisition: N samples of one channel at an
 *          exact sample rate (one conversion per timer interrupt), the
 *          front end of every measurement (FFT, frequency, RMS...).
 *
 * Samples are returned centred (raw - 2048) as int16, ready for dsp.h.
 * Uses TIM2 by default. Practical limit with interrupt-driven single
 * conversions: ~40 kHz on the F100 at 24 MHz.
 */
#ifndef ADC_CAPTURE_H
#define ADC_CAPTURE_H

#include "hal_types.h"
#include "hal_timer.h"

#define ADC_CAPTURE_TIMER  HAL_TIMER2
#define ADC_CAPTURE_MID    (2048)

/** Blocking capture. Returns HAL_ETIMEOUT if the rate is too high. */
hal_status_t adc_capture(uint8_t channel, uint32_t rate_hz, int16_t *buf, uint32_t n);

/** One-call measurements on a capture (all voltages in mV). */
typedef struct {
    uint32_t freq_centihz;   /**< zero-crossing frequency */
    uint32_t vpp_mv;
    uint32_t vrms_ac_mv;     /**< RMS after removing DC  */
    int32_t  vdc_mv;         /**< mean                   */
    int16_t  min_raw;
    int16_t  max_raw;
} adc_measure_t;

/** Analyse samples in place (they are DC-removed afterwards). */
void adc_capture_analyse(int16_t *buf, uint32_t n, uint32_t rate_hz, adc_measure_t *m);

#endif /* ADC_CAPTURE_H */

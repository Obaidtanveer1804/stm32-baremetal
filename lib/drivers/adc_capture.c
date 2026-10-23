/**
 * @file    adc_capture.c
 * @brief   Timer-paced ADC acquisition (see adc_capture.h).
 */
#include "adc_capture.h"
#include "hal_adc.h"
#include "hal_time.h"
#include "dsp.h"

#define CAPTURE_MARGIN_MS  (500U)

static int16_t          *s_buf;
static volatile uint32_t s_count;
static uint32_t          s_len;
static uint8_t           s_channel;

static void capture_isr(hal_timer_t t, void *arg)
{
    (void)t;
    (void)arg;
    if (s_count < s_len) {
        s_buf[s_count] = (int16_t)((int32_t)hal_adc_read(s_channel) - ADC_CAPTURE_MID);
        s_count++;
    }
}

hal_status_t adc_capture(uint8_t channel, uint32_t rate_hz, int16_t *buf, uint32_t n)
{
    if ((buf == NULL) || (n == 0U) || (rate_hz == 0U)) {
        return HAL_EINVAL;
    }
    (void)hal_adc_init();
    (void)hal_adc_read(channel);              /* pin to analog mode */
    s_buf     = buf;
    s_len     = n;
    s_channel = channel;
    s_count   = 0U;
    const hal_status_t st = hal_timer_start_periodic(ADC_CAPTURE_TIMER, rate_hz, capture_isr, NULL);
    if (st != HAL_OK) {
        return st;
    }
    const uint32_t budget = ((n * 1000U) / rate_hz) + CAPTURE_MARGIN_MS;
    const uint32_t t0     = hal_time_ms();
    while ((s_count < n) && ((hal_time_ms() - t0) <= budget)) {
        /* wait; the ISR fills the buffer */
    }
    hal_timer_stop(ADC_CAPTURE_TIMER);
    return (s_count < n) ? HAL_ETIMEOUT : HAL_OK;
}

static int32_t raw_to_mv_signed(int32_t raw)
{
    const int32_t mv = (int32_t)hal_adc_raw_to_mv((uint32_t)((raw < 0) ? -raw : raw), HAL_ADC_VREF_MV);
    return (raw < 0) ? -mv : mv;
}

void adc_capture_analyse(int16_t *buf, uint32_t n, uint32_t rate_hz, adc_measure_t *m)
{
    dsp_minmax(buf, n, &m->min_raw, &m->max_raw);
    m->vdc_mv = raw_to_mv_signed(dsp_mean(buf, n) + ADC_CAPTURE_MID);
    m->vpp_mv = hal_adc_raw_to_mv((uint32_t)(m->max_raw - m->min_raw), HAL_ADC_VREF_MV);
    dsp_remove_dc(buf, n);
    m->vrms_ac_mv   = hal_adc_raw_to_mv(dsp_rms(buf, n), HAL_ADC_VREF_MV);
    m->freq_centihz = dsp_zero_cross_freq_c(buf, n, rate_hz);
}

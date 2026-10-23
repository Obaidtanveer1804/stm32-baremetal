/**
 * @file    adc_pc.c
 * @brief   Simulated ADC1 and DAC.
 *
 * Every channel has a signal source: constant, or a waveform computed
 * from the simulated time (sine, triangle, square, ramp, noise).
 * The temperature channel follows sim_set_temperature_c10() with the
 * F1 sensor law, VREFINT reads 1.20 V for a 3.3 V supply, and the DAC
 * pins PA4/PA5 are "wired" to ADC channels 4/5 unless overridden,
 * so a DAC -> ADC loopback test works out of the box.
 */
#include "sim_internal.h"
#include "fixmath.h"
#include "rand.h"
#include "strutil.h"
#include "board.h"

#define SIM_TEMP_V25_MV      (1430)
#define SIM_TEMP_SLOPE_UV    (4300)

typedef struct {
    sim_wave_t wave;
    bool       set;          /* a source was configured */
    uint16_t   center;
    uint16_t   amplitude;
    uint32_t   period_ms;
} adc_src_t;

static adc_src_t   s_src[HAL_ADC_CH_COUNT];
static uint16_t    s_noise_lsb;
static int32_t     s_temp_c10 = 250;
static rand_xs32_t s_rng = { 0x1234567U };
static uint16_t    s_dac[HAL_DAC_COUNT];
static bool        s_dac_on[HAL_DAC_COUNT];

static const char *const wave_names[SIM_WAVE_COUNT] = {
    "const", "sine", "triangle", "square", "ramp", "noise"
};

/* ------------------------------ sim API --------------------------- */

void sim_adc_set(uint8_t channel, uint16_t raw)
{
    sim_adc_wave(channel, SIM_WAVE_CONST, raw, 0U, 1000U);
}

void sim_adc_wave(uint8_t channel, sim_wave_t wave, uint16_t center,
                  uint16_t amplitude, uint32_t period_ms)
{
    if ((channel >= HAL_ADC_CH_COUNT) || ((uint32_t)wave >= SIM_WAVE_COUNT)) {
        return;
    }
    s_src[channel].wave      = wave;
    s_src[channel].center    = (center > HAL_ADC_MAX) ? (uint16_t)HAL_ADC_MAX : center;
    s_src[channel].amplitude = amplitude;
    s_src[channel].period_ms = (period_ms == 0U) ? 1U : period_ms;
    s_src[channel].set       = true;
}

void sim_adc_noise(uint16_t lsb)
{
    s_noise_lsb = lsb;
}

bool sim_parse_wave(const char *name, sim_wave_t *wave)
{
    for (uint32_t i = 0U; i < SIM_WAVE_COUNT; i++) {
        if (str_ieq(name, wave_names[i])) {
            *wave = (sim_wave_t)i;
            return true;
        }
    }
    return false;
}

const char *sim_wave_name(sim_wave_t wave)
{
    return ((uint32_t)wave < SIM_WAVE_COUNT) ? wave_names[wave] : "?";
}

void sim_set_temperature_c10(int32_t c10)
{
    s_temp_c10 = c10;
}

int32_t sim_get_temperature_c10(void)
{
    return s_temp_c10;
}

/* --------------------------- signal model ------------------------- */

static int32_t wave_value(const adc_src_t *s, uint64_t now_us)
{
    /* microsecond time base: smooth waveforms even at kHz sample rates */
    const uint64_t period_us = (uint64_t)s->period_ms * 1000U;
    const uint32_t phase     = (uint32_t)(((now_us % period_us) * 65536U) / period_us);   /* 0..65535 */
    const int32_t  amp   = s->amplitude;

    switch (s->wave) {
    case SIM_WAVE_SINE:
        return s->center + ((amp * fx_sin_q15((uint16_t)phase)) / 32767);
    case SIM_WAVE_TRIANGLE: {
        /* -amp .. +amp .. -amp over one period */
        const int32_t tri = (phase < 32768U) ? ((int32_t)phase * 2 - 32768)
                                             : (32768 - ((int32_t)(phase - 32768U) * 2));
        return s->center + ((amp * tri) / 32768);
    }
    case SIM_WAVE_SQUARE:
        return s->center + ((phase < 32768U) ? amp : -amp);
    case SIM_WAVE_RAMP:
        return s->center - amp + (int32_t)(((uint32_t)amp * 2U * phase) / 65536U);
    case SIM_WAVE_NOISE:
        return rand_xs32_range(&s_rng, s->center - amp, s->center + amp);
    case SIM_WAVE_CONST:
    case SIM_WAVE_COUNT:
    default:
        return s->center;
    }
}

static uint16_t clamp_raw(int32_t v)
{
    return (uint16_t)((v < 0) ? 0 : ((v > (int32_t)HAL_ADC_MAX) ? (int32_t)HAL_ADC_MAX : v));
}

static uint16_t sample(uint8_t ch)
{
    int32_t v;

    if (s_src[ch].set) {
        v = wave_value(&s_src[ch], sim_event_time_us());
    } else if (ch == HAL_ADC_CH_TEMP) {
        const int32_t mv = SIM_TEMP_V25_MV - (((s_temp_c10 - 250) * SIM_TEMP_SLOPE_UV) / 10000);
        v = (mv * (int32_t)HAL_ADC_MAX) / (int32_t)HAL_ADC_VREF_MV;
    } else if (ch == HAL_ADC_CH_VREFINT) {
        v = (int32_t)((HAL_ADC_VREFINT_MV * HAL_ADC_MAX) / HAL_ADC_VREF_MV);
    } else if (((ch == 4U) || (ch == 5U)) && s_dac_on[ch - 4U]) {
        v = s_dac[ch - 4U];                    /* DAC pin wired to the ADC */
    } else {
        v = 0;                                 /* nothing connected */
    }
    if (s_noise_lsb != 0U) {
        v += rand_xs32_range(&s_rng, -(int32_t)s_noise_lsb, (int32_t)s_noise_lsb);
    }
    return clamp_raw(v);
}

/* ------------------------------ hal_adc --------------------------- */

hal_pin_t hal_adc_channel_pin(uint8_t channel)
{
    if (channel <= 7U)  { return PA(channel); }
    if (channel <= 9U)  { return PB(channel - 8U); }
    if (channel <= 15U) { return PC(channel - 10U); }
    return HAL_PIN_NONE;
}

hal_status_t hal_adc_init(void)
{
    sim_core_ensure_started();
    sim_log(SIM_LOG_ADC, "adc init (12 bit, simulated)");
    return HAL_OK;
}

uint16_t hal_adc_read(uint8_t channel)
{
    if (channel >= HAL_ADC_CH_COUNT) {
        return 0U;
    }
    const hal_pin_t pin = hal_adc_channel_pin(channel);
    if ((pin != HAL_PIN_NONE) && (hal_gpio_get_mode(pin) != HAL_GPIO_ANALOG)) {
        hal_gpio_mode(pin, HAL_GPIO_ANALOG);
    }
    const uint16_t raw = sample(channel);
    sim_log(SIM_LOG_ADC, "adc ch%u -> %u", (unsigned)channel, (unsigned)raw);
    return raw;
}

uint16_t hal_adc_read_avg(uint8_t channel, uint8_t log2_samples)
{
    uint32_t       sum = 0U;
    const uint32_t n   = 1UL << log2_samples;
    for (uint32_t i = 0U; i < n; i++) {
        sum += sample(channel);
    }
    return (uint16_t)((sum + (n / 2U)) >> log2_samples);
}

uint32_t hal_adc_vdda_mv(void)
{
    const uint32_t raw = hal_adc_read_avg(HAL_ADC_CH_VREFINT, 4U);
    return (raw == 0U) ? HAL_ADC_VREF_MV : ((HAL_ADC_VREFINT_MV * HAL_ADC_MAX) / raw);
}

uint32_t hal_adc_read_mv(uint8_t channel)
{
    return hal_adc_raw_to_mv(hal_adc_read(channel), hal_adc_vdda_mv());
}

int32_t hal_adc_temp_c10(void)
{
    const uint32_t raw = hal_adc_read_avg(HAL_ADC_CH_TEMP, 4U);
    return hal_adc_mv_to_temp_c10(hal_adc_raw_to_mv(raw, hal_adc_vdda_mv()));
}

void sim_adc_status(void)
{
    for (uint32_t ch = 0U; ch < HAL_ADC_CH_COUNT; ch++) {
        const adc_src_t *s = &s_src[ch];
        if (s->set) {
            sim_printf("ADC ch%-2u %s center %u amp %u period %lu ms\r\n", (unsigned)ch,
                       wave_names[s->wave], (unsigned)s->center, (unsigned)s->amplitude,
                       (unsigned long)s->period_ms);
        }
    }
    sim_printf("ADC temp sensor %ld.%ld C, noise +-%u LSB\r\n", (long)(s_temp_c10 / 10),
               (long)((s_temp_c10 < 0 ? -s_temp_c10 : s_temp_c10) % 10), (unsigned)s_noise_lsb);
    for (uint32_t d = 0U; d < HAL_DAC_COUNT; d++) {
        if (s_dac_on[d]) {
            sim_printf("DAC%u (PA%u) = %u\r\n", (unsigned)d + 1U, (unsigned)d + 4U, (unsigned)s_dac[d]);
        }
    }
}

/* ------------------------------ hal_dac --------------------------- */

hal_status_t hal_dac_init(hal_dac_t ch)
{
#if BOARD_HAS_DAC
    if (ch >= HAL_DAC_COUNT) {
        return HAL_EINVAL;
    }
    hal_gpio_mode((ch == HAL_DAC1) ? PA(4) : PA(5), HAL_GPIO_ANALOG);
    s_dac_on[ch] = true;
    return hal_dac_write(ch, 0U);
#else
    (void)ch;
    return HAL_EUNSUPPORTED;
#endif
}

hal_status_t hal_dac_write(hal_dac_t ch, uint16_t value)
{
    if ((ch >= HAL_DAC_COUNT) || !s_dac_on[ch]) {
        return (ch >= HAL_DAC_COUNT) ? HAL_EINVAL : HAL_ERROR;
    }
    s_dac[ch] = (value > HAL_DAC_MAX) ? (uint16_t)HAL_DAC_MAX : value;
    sim_log(SIM_LOG_ADC, "dac%u <- %u", (unsigned)ch + 1U, (unsigned)s_dac[ch]);
    return HAL_OK;
}

hal_status_t hal_dac_write_mv(hal_dac_t ch, uint32_t mv)
{
    const uint32_t raw = ((mv * HAL_DAC_MAX) + (HAL_ADC_VREF_MV / 2U)) / HAL_ADC_VREF_MV;
    return hal_dac_write(ch, (uint16_t)((raw > HAL_DAC_MAX) ? HAL_DAC_MAX : raw));
}

uint16_t hal_dac_get(hal_dac_t ch)
{
    return (ch < HAL_DAC_COUNT) ? s_dac[ch] : 0U;
}

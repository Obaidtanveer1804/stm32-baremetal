/**
 * @file    adc_f1.c
 * @brief   hal_adc on ADC1: software-triggered single conversions.
 */
#include "hal_adc.h"
#include "hal_gpio.h"
#include "hal_time.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>

#define ADC_MAX_CLOCK_HZ   (14000000U)   /* RM0008: ADCCLK <= 14 MHz */

static bool s_ready;

hal_pin_t hal_adc_channel_pin(uint8_t channel)
{
    if (channel <= 7U) {
        return PA(channel);
    }
    if (channel <= 9U) {
        return PB(channel - 8U);
    }
    if (channel <= 15U) {
        return PC(channel - 10U);
    }
    return HAL_PIN_NONE;
}

hal_status_t hal_adc_init(void)
{
    /* ADC clock = APB2 / {2,4,6,8}: pick the smallest divider <= 14 MHz */
    uint32_t adcpre = RCC_CFGR_ADCPRE_DIV8;
    if ((rcc_apb2_frequency / 2U) <= ADC_MAX_CLOCK_HZ) {
        adcpre = RCC_CFGR_ADCPRE_DIV2;
    } else if ((rcc_apb2_frequency / 4U) <= ADC_MAX_CLOCK_HZ) {
        adcpre = RCC_CFGR_ADCPRE_DIV4;
    } else if ((rcc_apb2_frequency / 6U) <= ADC_MAX_CLOCK_HZ) {
        adcpre = RCC_CFGR_ADCPRE_DIV6;
    } else {
        /* keep DIV8 */
    }
    rcc_set_adcpre(adcpre);
    rcc_periph_clock_enable(RCC_ADC1);

    adc_power_off(ADC1);
    adc_disable_scan_mode(ADC1);
    adc_set_single_conversion_mode(ADC1);
    adc_disable_external_trigger_regular(ADC1);
    adc_set_right_aligned(ADC1);
    /* longest sample time: high source impedance tolerated, and the
     * temperature sensor needs >= 17.1 us */
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5CYC);
    adc_enable_temperature_sensor();   /* also enables VREFINT (TSVREFE) */
    adc_power_on(ADC1);
    hal_delay_us(10U);                 /* tSTAB: ADC power-up time */
    adc_reset_calibration(ADC1);
    adc_calibrate(ADC1);
    s_ready = true;
    return HAL_OK;
}

uint16_t hal_adc_read(uint8_t channel)
{
    if (channel >= HAL_ADC_CH_COUNT) {
        return 0U;
    }
    if (!s_ready) {
        (void)hal_adc_init();
    }
    const hal_pin_t pin = hal_adc_channel_pin(channel);
    if ((pin != HAL_PIN_NONE) && (hal_gpio_get_mode(pin) != HAL_GPIO_ANALOG)) {
        hal_gpio_mode(pin, HAL_GPIO_ANALOG);
    }

    uint8_t seq[1] = { channel };
    adc_set_regular_sequence(ADC1, 1U, seq);
    adc_start_conversion_direct(ADC1);
    while (!adc_eoc(ADC1)) {
        /* ~20 us at 12 MHz with 239.5 cycles sampling */
    }
    return (uint16_t)adc_read_regular(ADC1);
}

uint16_t hal_adc_read_avg(uint8_t channel, uint8_t log2_samples)
{
    uint32_t sum = 0U;
    const uint32_t n = 1UL << log2_samples;
    for (uint32_t i = 0U; i < n; i++) {
        sum += hal_adc_read(channel);
    }
    return (uint16_t)((sum + (n / 2U)) >> log2_samples);
}

uint32_t hal_adc_vdda_mv(void)
{
    /* VREFINT is a fixed ~1.20 V: VDDA = 1200 * 4095 / raw */
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

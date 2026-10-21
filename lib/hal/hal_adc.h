/**
 * @file    hal_adc.h
 * @brief   12-bit ADC1: single conversions, millivolts, internal
 *          temperature sensor and supply voltage measurement.
 *
 *   channel  0..7  -> PA0..PA7
 *   channel  8..9  -> PB0..PB1
 *   channel 10..15 -> PC0..PC5
 *   channel 16     -> internal temperature sensor
 *   channel 17     -> internal reference VREFINT (~1.20 V)
 */
#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "hal_types.h"
#include "hal_gpio.h"

#define HAL_ADC_CH_TEMP      (16U)
#define HAL_ADC_CH_VREFINT   (17U)
#define HAL_ADC_CH_COUNT     (18U)
#define HAL_ADC_MAX          (4095U)
#define HAL_ADC_VREF_MV      (3300U)   /* nominal VDDA */
#define HAL_ADC_VREFINT_MV   (1200U)   /* datasheet typical */

hal_status_t hal_adc_init(void);
/** One blocking conversion, 0..4095. Configures the pin as analog. */
uint16_t     hal_adc_read(uint8_t channel);
/** Average of 2^n conversions (oversampling for less noise). */
uint16_t     hal_adc_read_avg(uint8_t channel, uint8_t log2_samples);
/** Raw value converted with the measured VDDA, in millivolts. */
uint32_t     hal_adc_read_mv(uint8_t channel);
/** VDDA in mV, measured through VREFINT (no external reference). */
uint32_t     hal_adc_vdda_mv(void);
/** Die temperature in tenths of a degree C (235 = 23.5 C, +-1.5 C). */
int32_t      hal_adc_temp_c10(void);
/** Pin used by a channel, HAL_PIN_NONE for internal channels. */
hal_pin_t    hal_adc_channel_pin(uint8_t channel);

/** Portable conversion helpers (unit-tested). */
uint32_t hal_adc_raw_to_mv(uint32_t raw, uint32_t vdda_mv);
/** F1 sensor: T = (V25 - Vsense) / slope + 25, V25 = 1430 mV, 4.3 mV/C */
int32_t  hal_adc_mv_to_temp_c10(uint32_t sense_mv);

#endif /* HAL_ADC_H */

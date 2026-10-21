/**
 * @file    hal_dac.h
 * @brief   12-bit DAC (channel 1 = PA4, channel 2 = PA5).
 *
 * Present on the STM32F100 (VL-Discovery), absent on the STM32F103C8/RB
 * (medium density): there every call returns HAL_EUNSUPPORTED.
 */
#ifndef HAL_DAC_H
#define HAL_DAC_H

#include "hal_types.h"

#define HAL_DAC_MAX   (4095U)

typedef enum {
    HAL_DAC1 = 0,
    HAL_DAC2,
    HAL_DAC_COUNT
} hal_dac_t;

hal_status_t hal_dac_init(hal_dac_t ch);
hal_status_t hal_dac_write(hal_dac_t ch, uint16_t value);
hal_status_t hal_dac_write_mv(hal_dac_t ch, uint32_t mv);
uint16_t     hal_dac_get(hal_dac_t ch);

#endif /* HAL_DAC_H */

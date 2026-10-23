/**
 * @file    dac_f1.c
 * @brief   hal_dac on the STM32F100 DAC (PA4 / PA5). Stubs elsewhere.
 */
#include "hal_dac.h"
#include "hal_adc.h"
#include "hal_gpio.h"
#include "board.h"

#if BOARD_HAS_DAC

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dac.h>

static const int       dac_channel[HAL_DAC_COUNT] = { DAC_CHANNEL1, DAC_CHANNEL2 };
static const hal_pin_t dac_pin[HAL_DAC_COUNT]     = { PA(4), PA(5) };
static uint16_t        s_value[HAL_DAC_COUNT];

hal_status_t hal_dac_init(hal_dac_t ch)
{
    if (ch >= HAL_DAC_COUNT) {
        return HAL_EINVAL;
    }
    rcc_periph_clock_enable(RCC_DAC);
    /* analog mode avoids parasitic consumption (RM0008 12.2) */
    hal_gpio_mode(dac_pin[ch], HAL_GPIO_ANALOG);
    dac_disable(DAC1, dac_channel[ch]);
    dac_buffer_enable(DAC1, dac_channel[ch]);
    dac_trigger_disable(DAC1, dac_channel[ch]);
    dac_enable(DAC1, dac_channel[ch]);
    return hal_dac_write(ch, 0U);
}

hal_status_t hal_dac_write(hal_dac_t ch, uint16_t value)
{
    if (ch >= HAL_DAC_COUNT) {
        return HAL_EINVAL;
    }
    if (value > HAL_DAC_MAX) {
        value = HAL_DAC_MAX;
    }
    /* trigger disabled: DHR is copied to DOR one APB cycle later */
    dac_load_data_buffer_single(DAC1, value, DAC_ALIGN_RIGHT12, dac_channel[ch]);
    s_value[ch] = value;
    return HAL_OK;
}

uint16_t hal_dac_get(hal_dac_t ch)
{
    return (ch < HAL_DAC_COUNT) ? s_value[ch] : 0U;
}

#else  /* no DAC on this chip */

hal_status_t hal_dac_init(hal_dac_t ch)
{
    (void)ch;
    return HAL_EUNSUPPORTED;
}

hal_status_t hal_dac_write(hal_dac_t ch, uint16_t value)
{
    (void)ch;
    (void)value;
    return HAL_EUNSUPPORTED;
}

uint16_t hal_dac_get(hal_dac_t ch)
{
    (void)ch;
    return 0U;
}

#endif

hal_status_t hal_dac_write_mv(hal_dac_t ch, uint32_t mv)
{
    const uint32_t vdda = HAL_ADC_VREF_MV;
    const uint32_t raw  = ((mv * HAL_DAC_MAX) + (vdda / 2U)) / vdda;
    return hal_dac_write(ch, (uint16_t)((raw > HAL_DAC_MAX) ? HAL_DAC_MAX : raw));
}

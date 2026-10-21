/**
 * @file    wdg_f1.c
 * @brief   hal_wdg on the IWDG.
 */
#include "hal_wdg.h"

#include <libopencm3/stm32/iwdg.h>

#define IWDG_MAX_TIMEOUT_MS  (26214U)   /* 4095 * 256 / 40 kHz */

static bool     s_running;
static uint32_t s_timeout_ms;

hal_status_t hal_wdg_start(uint32_t timeout_ms)
{
    if ((timeout_ms == 0U) || (timeout_ms > IWDG_MAX_TIMEOUT_MS)) {
        return HAL_EINVAL;
    }
    if (s_running) {
        return HAL_EBUSY;      /* the IWDG cannot be reconfigured safely */
    }
    iwdg_set_period_ms(timeout_ms);
    iwdg_start();
    s_running    = true;
    s_timeout_ms = timeout_ms;
    return HAL_OK;
}

void hal_wdg_kick(void)
{
    if (s_running) {
        iwdg_reset();
    }
}

bool hal_wdg_is_running(void)
{
    return s_running;
}

uint32_t hal_wdg_timeout_ms(void)
{
    return s_timeout_ms;
}

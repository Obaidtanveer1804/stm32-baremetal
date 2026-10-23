/**
 * @file    hal_wdg.h
 * @brief   Independent watchdog (IWDG, runs on the 40 kHz LSI).
 *
 * Once started it cannot be stopped (hardware rule): the program must
 * call hal_wdg_kick() more often than the timeout, otherwise the chip
 * resets. hal_sys_reset_cause() then reports HAL_RESET_IWDG.
 * On the PC port a missed kick prints a message and exits (code 3).
 */
#ifndef HAL_WDG_H
#define HAL_WDG_H

#include "hal_types.h"

hal_status_t hal_wdg_start(uint32_t timeout_ms);
void         hal_wdg_kick(void);
bool         hal_wdg_is_running(void);
uint32_t     hal_wdg_timeout_ms(void);

#endif /* HAL_WDG_H */

/**
 * @file    hal_rtc.h
 * @brief   Real-time clock: the F1 RTC is a 32-bit seconds counter.
 *          Convert to a calendar with core/datetime.h.
 *
 * Clock source: LSE (32.768 kHz crystal) if BOARD_HAS_LSE, otherwise
 * the LSI (~40 kHz, +-10 %: fine for uptime, not for a clock).
 */
#ifndef HAL_RTC_H
#define HAL_RTC_H

#include "hal_types.h"

typedef void (*hal_rtc_alarm_cb_t)(void *arg);

hal_status_t hal_rtc_init(void);
uint32_t     hal_rtc_get_seconds(void);
void         hal_rtc_set_seconds(uint32_t seconds);
/** Fire @p cb once when the counter reaches @p seconds. */
hal_status_t hal_rtc_set_alarm(uint32_t seconds, hal_rtc_alarm_cb_t cb, void *arg);
void         hal_rtc_cancel_alarm(void);

#endif /* HAL_RTC_H */

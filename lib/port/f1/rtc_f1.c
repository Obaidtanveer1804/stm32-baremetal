/**
 * @file    rtc_f1.c
 * @brief   hal_rtc on the F1 RTC (backup domain, 32-bit counter, 1 Hz).
 */
#include "hal_rtc.h"
#include "board.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/cm3/nvic.h>

#define RTC_PRESCALE_LSE  (32767U)   /* 32768 Hz -> 1 Hz */
#define RTC_PRESCALE_LSI  (39999U)   /* ~40 kHz  -> 1 Hz */

static hal_rtc_alarm_cb_t s_alarm_cb;
static void              *s_alarm_arg;

hal_status_t hal_rtc_init(void)
{
    /* rtc_auto_awake keeps the counter if the RTC already runs (e.g.
     * after a reset with VBAT), otherwise it starts it from scratch */
#if BOARD_HAS_LSE
    rtc_auto_awake(RCC_LSE, RTC_PRESCALE_LSE);
#else
    /* The LSI lives in RCC_CSR, outside the backup domain: a system reset
     * switches it off while the RTC configuration survives. Restart it
     * first, otherwise the counter is frozen after every reset. */
    rcc_osc_on(RCC_LSI);
    rcc_wait_for_osc_ready(RCC_LSI);
    rtc_auto_awake(RCC_LSI, RTC_PRESCALE_LSI);
#endif
    return HAL_OK;
}

uint32_t hal_rtc_get_seconds(void)
{
    return rtc_get_counter_val();
}

void hal_rtc_set_seconds(uint32_t seconds)
{
    rtc_set_counter_val(seconds);
}

hal_status_t hal_rtc_set_alarm(uint32_t seconds, hal_rtc_alarm_cb_t cb, void *arg)
{
    s_alarm_cb  = cb;
    s_alarm_arg = arg;
    rtc_set_alarm_time(seconds);
    rtc_clear_flag(RTC_ALR);
    rtc_interrupt_enable(RTC_ALR);
    nvic_enable_irq(NVIC_RTC_IRQ);
    return HAL_OK;
}

void hal_rtc_cancel_alarm(void)
{
    rtc_interrupt_disable(RTC_ALR);
    s_alarm_cb = NULL;
}

void rtc_isr(void)
{
    if (rtc_check_flag(RTC_ALR) != 0U) {
        rtc_clear_flag(RTC_ALR);
        rtc_interrupt_disable(RTC_ALR);      /* one-shot */
        if (s_alarm_cb != NULL) {
            s_alarm_cb(s_alarm_arg);
        }
    }
}

/**
 * @file    time_f1.c
 * @brief   hal_time on SysTick: 1 ms interrupt + sub-ms interpolation.
 */
#include "hal_time.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/cortex.h>

#define TICKS_PER_SECOND   (1000U)

#define LOOP_CYCLES        (4U)      /* approx. cycles per busy-loop turn */

static volatile uint32_t s_ms;
static uint32_t          s_reload;
static hal_tick_hook_t   s_hook;
static bool              s_running;

/* Used before hal_time_init() (drivers may need short delays during
 * early init) and for ms delays with interrupts masked.
 * Accuracy ~+-30 %, never hangs. */
static void busy_wait_us(uint32_t us)
{
    const uint32_t turns = (rcc_ahb_frequency / 1000000U / LOOP_CYCLES) * us;
    for (volatile uint32_t i = 0U; i < turns; i++) {
        __asm__ volatile ("nop");
    }
}

void hal_time_init(void)
{
    s_reload = (rcc_ahb_frequency / TICKS_PER_SECOND) - 1U;
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(s_reload);
    systick_clear();
    systick_interrupt_enable();
    systick_counter_enable();
    s_running = true;
}

/* Overrides the weak handler of libopencm3's vector table. */
void sys_tick_handler(void)
{
    s_ms++;
    if (s_hook != NULL) {
        s_hook(s_ms);
    }
}

uint32_t hal_time_ms(void)
{
    return s_ms;   /* 32-bit aligned read is atomic on Cortex-M */
}

uint32_t hal_time_us(void)
{
    uint32_t ms;
    uint32_t v1;
    uint32_t v2;
    bool     pending;

    /* SysTick counts DOWN from reload to 0. If interrupts are masked the
     * tick ISR cannot run, so a wrap shows up only as the PENDSTSET flag:
     * take it into account, otherwise time would jump back by 1 ms. */
    do {
        ms      = s_ms;
        v1      = systick_get_value();
        pending = (SCB_ICSR & SCB_ICSR_PENDSTSET) != 0U;
        v2      = systick_get_value();
    } while (ms != s_ms);              /* the ISR ran meanwhile: retry */

    uint32_t val;
    if (v2 > v1) {
        ms++;                          /* wrapped between the two reads */
        val = v2;
    } else {
        val = v1;
        if (pending) {
            ms++;                      /* wrapped before, tick not served yet */
        }
    }
    const uint32_t elapsed_ticks = s_reload - val;
    return (ms * 1000U) + ((elapsed_ticks * 1000U) / (s_reload + 1U));
}

void hal_delay_ms(uint32_t ms)
{
    if (!s_running) {
        busy_wait_us(ms * 1000U);
        return;
    }
    if (cm_is_masked_interrupts()) {
        /* the ms counter cannot advance, and after a second SysTick wrap
         * the pending flag cannot tell how many were missed: busy loop */
        busy_wait_us(ms * 1000U);
        return;
    }
    const uint32_t start = hal_time_ms();
    while ((hal_time_ms() - start) < ms) {
        /* spin: WFI would also wait for unrelated interrupts */
    }
}

void hal_delay_us(uint32_t us)
{
    if (!s_running) {
        busy_wait_us(us);
        return;
    }
    const uint32_t start = hal_time_us();
    while ((hal_time_us() - start) < us) {
    }
}

void hal_time_set_tick_hook(hal_tick_hook_t hook)
{
    s_hook = hook;
}

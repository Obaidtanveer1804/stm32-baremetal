/**
 * @file    timer_pc.c
 * @brief   Simulated TIM2..TIM4: same PSC/ARR arithmetic as the chip
 *          (24 MHz kernel clock), update "interrupts" delivered from the
 *          1 ms tick with nanosecond bookkeeping, so the average rate is
 *          exact even for frequencies above 1 kHz (several callbacks are
 *          then delivered per tick, which is the only difference).
 */
#include "sim_internal.h"

#define SIM_TIMER_CLK_HZ     (24000000ULL)
#define SIM_TIMER_MAX_BURST  (2000U)     /* callbacks per tick at most */

typedef struct {
    hal_timer_cb_t    cb;
    void             *arg;
    volatile uint32_t ticks;
    uint64_t          period_ns;
    uint64_t          start_ns;
    uint64_t          next_ns;
    uint32_t          freq_hz;
    uint16_t          psc;
    uint16_t          arr;
    bool              running;
    bool              irq;
} sim_tim_t;

static sim_tim_t s_tim[HAL_TIMER_COUNT];
static volatile uint64_t s_event_ns;      /* 0 = not inside a timer callback */

uint64_t sim_event_time_us(void)
{
    return (s_event_ns != 0U) ? (s_event_ns / 1000U) : sim_now_us();
}

uint32_t hal_timer_clock_hz(hal_timer_t timer)
{
    (void)timer;
    return (uint32_t)SIM_TIMER_CLK_HZ;
}

static void apply(hal_timer_t timer, uint16_t psc, uint16_t arr)
{
    sim_tim_t *t = &s_tim[timer];
    t->psc       = psc;
    t->arr       = arr;
    t->period_ns = (((uint64_t)psc + 1U) * ((uint64_t)arr + 1U) * 1000000000ULL) / SIM_TIMER_CLK_HZ;
    t->freq_hz   = (uint32_t)(SIM_TIMER_CLK_HZ / (((uint64_t)psc + 1U) * ((uint64_t)arr + 1U)));
}

hal_status_t sim_timer_setup_base(hal_timer_t timer, uint32_t freq_hz)
{
    uint16_t psc;
    uint16_t arr;

    if ((timer >= HAL_TIMER_COUNT) ||
        !hal_timer_calc((uint32_t)SIM_TIMER_CLK_HZ, freq_hz, &psc, &arr)) {
        return HAL_EINVAL;
    }
    sim_core_ensure_started();
    const uint32_t irq = hal_irq_save();
    apply(timer, psc, arr);
    s_tim[timer].start_ns = sim_now_us() * 1000U;
    s_tim[timer].next_ns  = s_tim[timer].start_ns + s_tim[timer].period_ns;
    s_tim[timer].running  = true;
    hal_irq_restore(irq);
    sim_log(SIM_LOG_TIMER, "TIM%u psc=%u arr=%u -> %lu Hz", (unsigned)timer + 2U,
            (unsigned)psc, (unsigned)arr, (unsigned long)s_tim[timer].freq_hz);
    return HAL_OK;
}

uint16_t sim_timer_arr(hal_timer_t timer)
{
    return s_tim[timer].arr;
}

hal_status_t hal_timer_start_periodic(hal_timer_t timer, uint32_t freq_hz,
                                      hal_timer_cb_t cb, void *arg)
{
    if (timer >= HAL_TIMER_COUNT) {
        return HAL_EINVAL;
    }
    const hal_status_t st = sim_timer_setup_base(timer, freq_hz);
    if (st == HAL_OK) {
        const uint32_t irq = hal_irq_save();
        s_tim[timer].cb    = cb;
        s_tim[timer].arg   = arg;
        s_tim[timer].ticks = 0U;
        s_tim[timer].irq   = true;
        hal_irq_restore(irq);
    }
    return st;
}

void hal_timer_stop(hal_timer_t timer)
{
    if (timer < HAL_TIMER_COUNT) {
        const uint32_t irq = hal_irq_save();
        s_tim[timer].running = false;
        s_tim[timer].irq     = false;
        s_tim[timer].cb      = NULL;
        hal_irq_restore(irq);
        sim_log(SIM_LOG_TIMER, "TIM%u stopped", (unsigned)timer + 2U);
    }
}

bool hal_timer_is_running(hal_timer_t timer)
{
    return (timer < HAL_TIMER_COUNT) && s_tim[timer].running;
}

hal_status_t hal_timer_set_frequency(hal_timer_t timer, uint32_t freq_hz)
{
    uint16_t psc;
    uint16_t arr;
    if ((timer >= HAL_TIMER_COUNT) ||
        !hal_timer_calc((uint32_t)SIM_TIMER_CLK_HZ, freq_hz, &psc, &arr)) {
        return HAL_EINVAL;
    }
    const uint32_t irq = hal_irq_save();
    apply(timer, psc, arr);     /* takes effect from the next period */
    hal_irq_restore(irq);
    return HAL_OK;
}

uint32_t hal_timer_get_frequency(hal_timer_t timer)
{
    return (timer < HAL_TIMER_COUNT) ? s_tim[timer].freq_hz : 0U;
}

uint32_t hal_timer_get_ticks(hal_timer_t timer)
{
    return (timer < HAL_TIMER_COUNT) ? s_tim[timer].ticks : 0U;
}

uint16_t hal_timer_get_counter(hal_timer_t timer)
{
    if ((timer >= HAL_TIMER_COUNT) || !s_tim[timer].running) {
        return 0U;
    }
    const sim_tim_t *t       = &s_tim[timer];
    const uint64_t   tick_ns = (((uint64_t)t->psc + 1U) * 1000000000ULL) / SIM_TIMER_CLK_HZ;
    const uint64_t   elapsed = (sim_now_us() * 1000U) - t->start_ns;
    return (uint16_t)((elapsed / ((tick_ns == 0U) ? 1U : tick_ns)) % ((uint64_t)t->arr + 1U));
}

/* Deliver the update interrupts that became due. */
void sim_timer_tick(uint64_t now_us)
{
    const uint64_t now_ns = now_us * 1000U;
    uint32_t       burst  = 0U;

    /* deliver the due update events of all timers in time order, so a
     * generator on one timer and an ADC capture on another interleave
     * like they do on the chip */
    for (;;) {
        sim_tim_t *next = NULL;
        uint32_t   idx  = 0U;
        for (uint32_t i = 0U; i < HAL_TIMER_COUNT; i++) {
            sim_tim_t *t = &s_tim[i];
            if (t->running && (t->period_ns != 0U) && (t->next_ns <= now_ns) &&
                ((next == NULL) || (t->next_ns < next->next_ns))) {
                next = t;
                idx  = i;
            }
        }
        if (next == NULL) {
            break;
        }
        const uint64_t due_ns = next->next_ns;
        next->next_ns += next->period_ns;
        next->ticks++;
        if (next->irq && (next->cb != NULL)) {
            s_event_ns = (due_ns == 0U) ? 1U : due_ns;
            next->cb((hal_timer_t)idx, next->arg);
            s_event_ns = 0U;
        }
        if (++burst >= SIM_TIMER_MAX_BURST) {
            /* far behind (host was busy): resynchronise every timer */
            for (uint32_t i = 0U; i < HAL_TIMER_COUNT; i++) {
                if (s_tim[i].next_ns <= now_ns) {
                    s_tim[i].next_ns = now_ns + s_tim[i].period_ns;
                }
            }
            break;
        }
    }
}

void sim_timer_status(void)
{
    for (uint32_t i = 0U; i < HAL_TIMER_COUNT; i++) {
        const sim_tim_t *t = &s_tim[i];
        if (t->running) {
            sim_printf("TIM%u   %lu Hz (psc %u arr %u) ticks %lu%s\r\n", (unsigned)i + 2U,
                       (unsigned long)t->freq_hz, (unsigned)t->psc, (unsigned)t->arr,
                       (unsigned long)t->ticks, t->irq ? " irq" : "");
        }
    }
}

/* ------------------------- register model ------------------------- */

#define TIM2_BASE_ADDR   (0x40000000U)
#define TIM_STRIDE       (0x400U)
#define TIM_CR1          (0x00U)
#define TIM_DIER         (0x0CU)
#define TIM_SR           (0x10U)
#define TIM_CCMR1        (0x18U)
#define TIM_CCMR2        (0x1CU)
#define TIM_CCER         (0x20U)
#define TIM_CNT          (0x24U)
#define TIM_PSC          (0x28U)
#define TIM_ARR          (0x2CU)
#define TIM_CCR1         (0x34U)
#define TIM_CCR4         (0x40U)
#define TIM_LAST_REG     (0x4CU)
#define CCMR_PWM1_PRELOAD (0x68U)   /* OCxM = 110, OCxPE = 1 */

static bool decode(uint32_t addr, hal_timer_t *timer, uint32_t *off)
{
    if ((addr < TIM2_BASE_ADDR) || ((addr & 3U) != 0U)) {
        return false;
    }
    const uint32_t idx = (addr - TIM2_BASE_ADDR) / TIM_STRIDE;
    const uint32_t o   = (addr - TIM2_BASE_ADDR) % TIM_STRIDE;
    if ((idx >= HAL_TIMER_COUNT) || (o > TIM_LAST_REG)) {
        return false;
    }
    *timer = (hal_timer_t)idx;
    *off   = o;
    return true;
}

bool sim_timer_reg_read(uint32_t addr, uint32_t *value)
{
    hal_timer_t tm;
    uint32_t    off;
    if (!decode(addr, &tm, &off)) {
        return false;
    }
    const sim_tim_t *t = &s_tim[tm];
    uint32_t v = 0U;
    switch (off) {
    case TIM_CR1:  v = t->running ? 0x81U : 0U;  break;   /* ARPE | CEN */
    case TIM_DIER: v = t->irq ? 1U : 0U;         break;   /* UIE */
    case TIM_SR:   v = 0U;                       break;
    case TIM_CNT:  v = hal_timer_get_counter(tm); break;
    case TIM_PSC:  v = t->psc;                   break;
    case TIM_ARR:  v = t->arr;                   break;
    case TIM_CCMR1:
        v = (sim_pwm_enabled(tm, 1U) ? CCMR_PWM1_PRELOAD : 0U) | (sim_pwm_enabled(tm, 2U) ? (CCMR_PWM1_PRELOAD << 8) : 0U);
        break;
    case TIM_CCMR2:
        v = (sim_pwm_enabled(tm, 3U) ? CCMR_PWM1_PRELOAD : 0U) | (sim_pwm_enabled(tm, 4U) ? (CCMR_PWM1_PRELOAD << 8) : 0U);
        break;
    case TIM_CCER:
        for (uint32_t ch = 1U; ch <= 4U; ch++) {
            v |= sim_pwm_enabled(tm, ch) ? (1U << ((ch - 1U) * 4U)) : 0U;   /* CCxE */
        }
        break;
    default:
        if ((off >= TIM_CCR1) && (off <= TIM_CCR4)) {
            v = sim_pwm_ccr(tm, ((off - TIM_CCR1) / 4U) + 1U);
        }
        break;
    }
    *value = v;
    return true;
}

bool sim_timer_reg_write(uint32_t addr, uint32_t value)
{
    hal_timer_t tm;
    uint32_t    off;
    if (!decode(addr, &tm, &off)) {
        return false;
    }
    sim_tim_t     *t   = &s_tim[tm];
    const uint32_t irq = hal_irq_save();
    switch (off) {
    case TIM_PSC: apply(tm, (uint16_t)value, t->arr); break;   /* "poke TIM2+0x28 23" */
    case TIM_ARR: apply(tm, t->psc, (uint16_t)value); break;
    case TIM_CR1:
        if ((value & 1U) == 0U) {
            t->running = false;
        } else if (!t->running && (t->period_ns != 0U)) {
            t->running = true;
            t->next_ns = (sim_now_us() * 1000U) + t->period_ns;
        } else {
            /* unchanged */
        }
        break;
    default:
        break;                              /* other registers read-only here */
    }
    hal_irq_restore(irq);
    return true;
}

/**
 * @file    misc_pc.c
 * @brief   Simulated EXTI, watchdog, user flash and RTC.
 */
#define _POSIX_C_SOURCE 200809L

#include "sim_internal.h"

#include <stdio.h>
#include <time.h>

/* =============================== EXTI ============================= */

#define EXTI_LINES  (16U)

typedef struct {
    hal_exti_cb_t     cb;
    void             *arg;
    hal_pin_t         pin;
    hal_exti_edge_t   edge;
    bool              enabled;
    volatile uint32_t count;
} exti_slot_t;

static exti_slot_t s_exti[EXTI_LINES];

hal_status_t hal_exti_attach(hal_pin_t pin, hal_exti_edge_t edge,
                             hal_exti_cb_t cb, void *arg)
{
    if (!hal_gpio_pin_valid(pin) || (cb == NULL)) {
        return HAL_EINVAL;
    }
    exti_slot_t *s = &s_exti[HAL_PIN_NUM(pin)];
    if ((s->cb != NULL) && (s->pin != pin)) {
        return HAL_EBUSY;        /* same rule as the F1: one port per line */
    }
    const uint32_t irq = hal_irq_save();
    s->cb      = cb;
    s->arg     = arg;
    s->pin     = pin;
    s->edge    = edge;
    s->enabled = true;
    s->count   = 0U;
    hal_irq_restore(irq);
    char name[6];
    sim_log(SIM_LOG_EXTI, "exti%u on %s", (unsigned)HAL_PIN_NUM(pin), hal_gpio_name(pin, name));
    return HAL_OK;
}

void hal_exti_detach(hal_pin_t pin)
{
    if (hal_gpio_pin_valid(pin) && (s_exti[HAL_PIN_NUM(pin)].pin == pin)) {
        s_exti[HAL_PIN_NUM(pin)].cb      = NULL;
        s_exti[HAL_PIN_NUM(pin)].enabled = false;
    }
}

void hal_exti_enable(hal_pin_t pin, bool enable)
{
    if (hal_gpio_pin_valid(pin) && (s_exti[HAL_PIN_NUM(pin)].pin == pin)) {
        s_exti[HAL_PIN_NUM(pin)].enabled = enable;
    }
}

uint32_t hal_exti_count(hal_pin_t pin)
{
    return hal_gpio_pin_valid(pin) ? s_exti[HAL_PIN_NUM(pin)].count : 0U;
}

void sim_exti_on_input_change(hal_pin_t pin, bool old_level, bool new_level)
{
    exti_slot_t *s = &s_exti[HAL_PIN_NUM(pin)];

    if ((s->cb == NULL) || !s->enabled || (s->pin != pin) || (old_level == new_level)) {
        return;
    }
    const hal_exti_edge_t edge = new_level ? HAL_EXTI_RISING : HAL_EXTI_FALLING;
    if ((s->edge & edge) == 0U) {
        return;
    }
    s->count++;
    sim_log(SIM_LOG_EXTI, "exti%u %s edge", (unsigned)HAL_PIN_NUM(pin), new_level ? "rising" : "falling");
    /* run the callback like an ISR: with the tick masked */
    const uint32_t irq = hal_irq_save();
    s->cb(pin, s->arg);
    hal_irq_restore(irq);
}

/* ============================== watchdog ========================== */

static volatile bool     s_wdg_running;
static volatile uint32_t s_wdg_timeout_ms;
static volatile uint32_t s_wdg_last_kick;

hal_status_t hal_wdg_start(uint32_t timeout_ms)
{
    if ((timeout_ms == 0U) || (timeout_ms > 26214U)) {
        return HAL_EINVAL;
    }
    if (s_wdg_running) {
        return HAL_EBUSY;
    }
    s_wdg_timeout_ms = timeout_ms;
    s_wdg_last_kick  = hal_time_ms();
    s_wdg_running    = true;
    sim_log(SIM_LOG_SYS, "watchdog started, %lu ms", (unsigned long)timeout_ms);
    return HAL_OK;
}

void hal_wdg_kick(void)
{
    s_wdg_last_kick = hal_time_ms();
}

bool hal_wdg_is_running(void)
{
    return s_wdg_running;
}

uint32_t hal_wdg_timeout_ms(void)
{
    return s_wdg_timeout_ms;
}

void sim_wdg_tick(uint32_t now_ms)
{
    if (s_wdg_running && ((now_ms - s_wdg_last_kick) > s_wdg_timeout_ms)) {
        s_wdg_running = false;
        sim_printf("\r\n[sim] WATCHDOG not kicked for %lu ms\r\n", (unsigned long)s_wdg_timeout_ms);
        sim_reset_with_cause("iwdg");
    }
}

/* ============================= user flash ========================= */

#define SIM_FLASH_FILE   "flash_sim.bin"
#define SIM_FLASH_BASE   (0x08000000U + (128U * 1024U) - (HAL_FLASH_USER_PAGES * HAL_FLASH_PAGE_SIZE))
#define SIM_FLASH_SIZE   (HAL_FLASH_USER_PAGES * HAL_FLASH_PAGE_SIZE)

static uint8_t s_flash[SIM_FLASH_SIZE];
static bool    s_flash_loaded;
static bool    s_flash_dirty;

static void flash_load(void)
{
    if (s_flash_loaded) {
        return;
    }
    s_flash_loaded = true;
    for (size_t i = 0U; i < SIM_FLASH_SIZE; i++) {
        s_flash[i] = 0xFFU;                 /* erased state */
    }
    FILE *f = fopen(SIM_FLASH_FILE, "rb");
    if (f != NULL) {
        if (fread(s_flash, 1U, SIM_FLASH_SIZE, f) != SIM_FLASH_SIZE) {
            /* short or empty file: the rest stays erased */
        }
        (void)fclose(f);
    }
}

void sim_flash_save(void)
{
    if (!s_flash_dirty) {
        return;
    }
    FILE *f = fopen(SIM_FLASH_FILE, "wb");
    if (f != NULL) {
        (void)fwrite(s_flash, 1U, SIM_FLASH_SIZE, f);
        (void)fclose(f);
        s_flash_dirty = false;
    }
}

uint32_t hal_flash_user_base(void)
{
    return SIM_FLASH_BASE;
}

uint32_t hal_flash_user_size(void)
{
    return SIM_FLASH_SIZE;
}

static bool in_area(uint32_t addr, size_t len)
{
    return (len > 0U) && (addr >= SIM_FLASH_BASE) && ((addr + len) <= (SIM_FLASH_BASE + SIM_FLASH_SIZE));
}

hal_status_t hal_flash_erase_page(uint32_t addr)
{
    if (!in_area(addr, 1U)) {
        return HAL_EINVAL;
    }
    flash_load();
    const uint32_t page = (addr - SIM_FLASH_BASE) & ~(HAL_FLASH_PAGE_SIZE - 1U);
    for (uint32_t i = 0U; i < HAL_FLASH_PAGE_SIZE; i++) {
        s_flash[page + i] = 0xFFU;
    }
    s_flash_dirty = true;
    sim_log(SIM_LOG_FLASH, "erase page 0x%08lX", (unsigned long)(SIM_FLASH_BASE + page));
    sim_flash_save();
    return HAL_OK;
}

hal_status_t hal_flash_write(uint32_t addr, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    if (((addr & 1U) != 0U) || ((len & 1U) != 0U) || !in_area(addr, len)) {
        return HAL_EINVAL;
    }
    flash_load();
    for (size_t i = 0U; i < len; i += 2U) {
        const uint32_t off = (addr - SIM_FLASH_BASE) + (uint32_t)i;
        const uint16_t old = (uint16_t)(s_flash[off] | ((uint16_t)s_flash[off + 1U] << 8));
        const uint16_t val = (uint16_t)(p[i] | ((uint16_t)p[i + 1U] << 8));
        /* F1 rule: a half-word can be programmed only if erased (0xFFFF),
         * except programming 0x0000. Otherwise PGERR. */
        if ((old != 0xFFFFU) && (val != 0x0000U)) {
            sim_log(SIM_LOG_FLASH, "PGERR at 0x%08lX (not erased)", (unsigned long)(addr + i));
            sim_flash_save();
            return HAL_ERROR;
        }
        s_flash[off]      = (uint8_t)val;
        s_flash[off + 1U] = (uint8_t)(val >> 8);
    }
    s_flash_dirty = true;
    sim_log(SIM_LOG_FLASH, "write %lu bytes at 0x%08lX", (unsigned long)len, (unsigned long)addr);
    sim_flash_save();
    return HAL_OK;
}

void hal_flash_read(uint32_t addr, void *data, size_t len)
{
    uint8_t *dst = (uint8_t *)data;
    flash_load();
    for (size_t i = 0U; i < len; i++) {
        dst[i] = in_area(addr + (uint32_t)i, 1U) ? s_flash[addr + i - SIM_FLASH_BASE] : 0xFFU;
    }
}

bool sim_flash_mem_read8(uint32_t addr, uint8_t *value)
{
    if (!in_area(addr, 1U)) {
        return false;
    }
    flash_load();
    *value = s_flash[addr - SIM_FLASH_BASE];
    return true;
}

/* ================================ RTC ============================= */

static volatile uint32_t s_rtc_base_s;
static volatile uint32_t s_rtc_base_ms;
static bool              s_rtc_ready;
static volatile bool     s_alarm_on;
static volatile uint32_t s_alarm_s;
static hal_rtc_alarm_cb_t s_alarm_cb;
static void             *s_alarm_arg;

hal_status_t hal_rtc_init(void)
{
    if (!s_rtc_ready) {
        /* like a battery-backed RTC that was set before: host UTC time */
        s_rtc_base_s  = (uint32_t)time(NULL);
        s_rtc_base_ms = hal_time_ms();
        s_rtc_ready   = true;
    }
    return HAL_OK;
}

uint32_t hal_rtc_get_seconds(void)
{
    if (!s_rtc_ready) {
        (void)hal_rtc_init();
    }
    return s_rtc_base_s + ((hal_time_ms() - s_rtc_base_ms) / 1000U);
}

void hal_rtc_set_seconds(uint32_t seconds)
{
    s_rtc_base_s  = seconds;
    s_rtc_base_ms = hal_time_ms();
    s_rtc_ready   = true;
}

hal_status_t hal_rtc_set_alarm(uint32_t seconds, hal_rtc_alarm_cb_t cb, void *arg)
{
    const uint32_t irq = hal_irq_save();
    s_alarm_s   = seconds;
    s_alarm_cb  = cb;
    s_alarm_arg = arg;
    s_alarm_on  = true;
    hal_irq_restore(irq);
    return HAL_OK;
}

void hal_rtc_cancel_alarm(void)
{
    s_alarm_on = false;
}

void sim_rtc_tick(void)
{
    if (s_alarm_on && s_rtc_ready && (hal_rtc_get_seconds() >= s_alarm_s)) {
        s_alarm_on = false;
        if (s_alarm_cb != NULL) {
            s_alarm_cb(s_alarm_arg);
        }
    }
}

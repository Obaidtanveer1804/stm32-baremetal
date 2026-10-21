/**
 * @file    sim_internal.h
 * @brief   Private glue between the files of the PC port.
 */
#ifndef SIM_INTERNAL_H
#define SIM_INTERNAL_H

#include "sim.h"

/* sim_core_pc.c ----------------------------------------------------- */
/** Ensure the tick engine runs (idempotent, called by hal_* functions). */
void     sim_core_ensure_started(void);
/** Raw, async-signal-safe output of a buffer (bypasses stdio). */
void     sim_raw_write(const char *data, size_t len);
/** Monotonic microseconds since the program started. */
uint64_t sim_now_us(void);
/** Time at which the current event "really" happens: inside a timer
 *  callback delivered late (several per 1 ms tick), the scheduled time
 *  of that update event; otherwise now. Used to sample signals. */
uint64_t sim_event_time_us(void);
/** True while executing inside the simulated interrupt (signal). */
bool     sim_in_isr(void);
/** True once stdin reached end of file (piped input consumed). */
bool     sim_stdin_eof(void);
bool     sim_stdin_is_tty(void);
/** printf straight to the terminal (async-signal safe, no io.h needed). */
void     sim_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/** Simulated reset ("software", "iwdg"): re-executes the program. */
void     sim_reset_with_cause(const char *cause) __attribute__((noreturn));

/* Per-millisecond hooks run from the tick, in this order. */
void sim_uart_tick(void);              /* stdin -> console RX buffer  */
void sim_timer_tick(uint64_t now_us);  /* hal_timer periodic callbacks */
void sim_rtc_tick(void);
void sim_wdg_tick(uint32_t now_ms);
void sim_time_tick(uint32_t now_ms);   /* hal_time tick hook          */

/* gpio_pc.c ---------------------------------------------------------- */
/** Notified on every output change (SPI chip-select tracking). */
void sim_spi_on_pin_change(hal_pin_t pin, bool level);
/** Notified on every input level change (EXTI edge detection). */
void sim_exti_on_input_change(hal_pin_t pin, bool old_level, bool new_level);
/** Register-level access for hal_sys_mem_* (GPIOx_IDR/ODR/BSRR). */
bool sim_gpio_reg_read(uint32_t addr, uint32_t *value);
bool sim_gpio_reg_write(uint32_t addr, uint32_t value);
void sim_gpio_status(void);

/* timer_pc.c: base shared with pwm_pc.c (like timer_f1.h) */
hal_status_t sim_timer_setup_base(hal_timer_t timer, uint32_t freq_hz);
uint16_t     sim_timer_arr(hal_timer_t timer);

/* register views of the timers (TIM2..4 at their F1 addresses) */
bool     sim_timer_reg_read(uint32_t addr, uint32_t *value);
bool     sim_timer_reg_write(uint32_t addr, uint32_t value);
/** CCR of a PWM channel (1..4) of a timer, 0 if unused. */
uint16_t sim_pwm_ccr(hal_timer_t timer, uint32_t channel);
bool     sim_pwm_enabled(hal_timer_t timer, uint32_t channel);

/* other modules' status printers (sim_status) */
void sim_pwm_status(void);
void sim_timer_status(void);
void sim_adc_status(void);
void sim_flash_save(void);

/* flash_pc.c */
bool sim_flash_mem_read8(uint32_t addr, uint8_t *value);

#endif /* SIM_INTERNAL_H */

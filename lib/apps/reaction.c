/**
 * @file    reaction.c
 * @brief   Reaction-time game: the LED lights after a random delay,
 *          press the button as fast as possible. Timing is done in the
 *          EXTI interrupt with the microsecond clock; statistics over
 *          the rounds with stats.h. Pressing too early is detected.
 *
 * PC: press Enter instead of the button.
 */
#include "hal.h"
#include "io.h"
#include "rand.h"
#include "stats.h"

#define ROUNDS          (5U)
#define DELAY_MIN_MS    (1000)
#define DELAY_MAX_MS    (3000)
#define TIMEOUT_MS      (3000U)

static volatile uint32_t s_press_us;
static volatile bool     s_pressed;

static void button_isr(hal_pin_t pin, void *arg)
{
    (void)pin;
    (void)arg;
    if (!s_pressed) {
        s_press_us = hal_time_us();     /* timestamp in the ISR: no latency */
        s_pressed  = true;
    }
}

/* the console Enter key works as a second button */
static bool console_pressed(void)
{
    const int c = io_getc();
    if ((c == '\r') || (c == '\n')) {
        if (!s_pressed) {
            s_press_us = hal_time_us();
            s_pressed  = true;
        }
        return true;
    }
    return c == IO_CLOSED;
}

int main(void)
{
    stats_t     st;
    rand_xs32_t rng;

    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    hal_gpio_mode(BOARD_LED_GREEN, HAL_GPIO_OUTPUT);
    hal_gpio_mode(BOARD_BUTTON, (BOARD_BUTTON_ACTIVE_HIGH != 0) ? HAL_GPIO_INPUT_PULLDOWN : HAL_GPIO_INPUT_PULLUP);
    (void)hal_exti_attach(BOARD_BUTTON, (BOARD_BUTTON_ACTIVE_HIGH != 0) ? HAL_EXTI_RISING : HAL_EXTI_FALLING,
                          button_isr, NULL);
    (void)hal_adc_init();
    /* noisy seed: ADC LSBs of the temperature sensor + time */
    rand_xs32_seed(&rng, hal_time_us() ^ ((uint32_t)hal_adc_read(HAL_ADC_CH_TEMP) << 12));
    stats_reset(&st);

    io_println("Reaction test: press the button (Enter) when the LED turns on.");
    for (uint32_t round = 1U; round <= ROUNDS; round++) {
        io_printf("round %lu: wait..." IO_NEWLINE, (unsigned long)round);
        hal_gpio_clear(BOARD_LED_GREEN);
        s_pressed = false;
        const uint32_t wait = (uint32_t)rand_xs32_range(&rng, DELAY_MIN_MS, DELAY_MAX_MS);
        const uint32_t t0   = hal_time_ms();
        bool           early = false;
        while ((hal_time_ms() - t0) < wait) {
            (void)console_pressed();
            if (s_pressed) {
                early = true;
                break;
            }
        }
        if (early) {
            io_println("  too early!");
            hal_delay_ms(500U);
            continue;
        }
        hal_gpio_set(BOARD_LED_GREEN);
        io_println("  NOW!");
        const uint32_t on_us = hal_time_us();
        while (!s_pressed && ((hal_time_us() - on_us) < (TIMEOUT_MS * 1000U))) {
            if (console_pressed() && !s_pressed) {
                break;
            }
        }
        if (!s_pressed) {
            io_println("  too slow");
            continue;
        }
        const uint32_t ms = (s_press_us - on_us) / 1000U;
        stats_add(&st, (int32_t)ms);
        io_printf("  %lu ms" IO_NEWLINE, (unsigned long)ms);
    }
    hal_gpio_clear(BOARD_LED_GREEN);
    stats_print(&st, "reaction ms");
    for (;;) {
        hal_sys_idle();
    }
    return 0;
}

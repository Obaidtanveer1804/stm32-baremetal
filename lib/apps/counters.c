/**
 * @file    counters.c
 * @brief   TP2 rewritten on the library: decimal, binary and hex
 *          counters, plus the LEDs as a 2-bit hardware counter.
 *
 * Same output as main_led.c + tp2.c, but:
 *  - counting logic (counter.c) and display (io.h) are hardware free,
 *  - delay is a real 200 ms (SysTick), not a NOP loop,
 *  - identical binary runs on the PC: make run APP=counters
 */
#include "hal.h"
#include "io.h"
#include "counter.h"
#include "bits.h"

#define COUNT_PERIOD_MS   (200U)
#define COUNT_BITS        (8U)
#define COUNT_MAX         ((int32_t)BIT_MASK(COUNT_BITS))

int main(void)
{
    static const hal_pin_t leds[] = BOARD_LEDS_INIT;
    counter_t c;

    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    for (uint32_t i = 0U; i < ARRAY_SIZE(leds); i++) {
        hal_gpio_mode(leds[i], HAL_GPIO_OUTPUT);
    }

    counter_init(&c, 0, COUNT_MAX, 1, COUNTER_MODE_WRAP);
    for (;;) {
        const uint32_t value = (uint32_t)counter_get(&c);
        counter_print_decimal(value, 3U);
        counter_print_binary(value, COUNT_BITS);
        counter_print_hex(value, 2U);
        /* bit k of the counter on LED k: one BSRR write per LED */
        for (uint32_t i = 0U; i < ARRAY_SIZE(leds); i++) {
            hal_gpio_write(leds[i], ((value >> i) & 1U) != 0U);
        }
        (void)counter_step(&c);
        hal_delay_ms(COUNT_PERIOD_MS);
    }
    return 0;
}

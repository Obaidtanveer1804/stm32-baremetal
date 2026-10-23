/**
 * @file    cmd_counter.c
 * @brief   The TP2 counters as a command, on the generic counter module:
 *
 *   count                      0..255, all bases, 200 ms, until q
 *   count 0 15 1 500 bin       binary only, 4 bits
 *   count 10 0 -1 1000 dec     count down
 *   count 0 255 1 50 leds      show the 2 LSBs on the board LEDs
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "counter.h"
#include "bits.h"
#include "fixmath.h"
#include "strutil.h"

typedef enum {
    SHOW_ALL = 0,
    SHOW_DEC,
    SHOW_HEX,
    SHOW_BIN,
    SHOW_BCD,
    SHOW_GRAY,
    SHOW_LEDS
} show_t;

static uint32_t bits_needed(int32_t lo, int32_t hi)
{
    const uint32_t m = (uint32_t)((fx_abs(lo) > fx_abs(hi)) ? fx_abs(lo) : fx_abs(hi));
    return (m == 0U) ? 1U : ((uint32_t)bits_msb_index(m) + 1U);
}

static int cmd_count(int argc, char *argv[])
{
    static const char *const shows[] = { "all", "dec", "hex", "bin", "bcd", "gray", "leds" };
    static const hal_pin_t   leds[]  = BOARD_LEDS_INIT;
    int32_t  lo = 0;
    int32_t  hi = 255;
    int32_t  step = 1;
    uint32_t period = 200U;
    show_t   show = SHOW_ALL;

    if ((argc >= 2) && !cli_arg_i32(argv[1], &lo)) { return CLI_ERR; }
    if ((argc >= 3) && !cli_arg_i32(argv[2], &hi)) { return CLI_ERR; }
    if ((argc >= 4) && !cli_arg_i32(argv[3], &step)) { return CLI_ERR; }
    if ((argc >= 5) && !cli_arg_u32_range(argv[4], 1U, 60000U, &period)) { return CLI_ERR; }
    if (argc >= 6) {
        bool found = false;
        for (uint32_t i = 0U; i < (sizeof(shows) / sizeof(shows[0])); i++) {
            if (str_eq(argv[5], shows[i])) {
                show  = (show_t)i;
                found = true;
            }
        }
        if (!found) {
            return cli_error("display: all dec hex bin bcd gray leds");
        }
    }
    if (step == 0) {
        return cli_error("step must not be 0");
    }

    counter_t c;
    /* "count 10 0 -1": a reversed range means counting down */
    counter_init(&c, lo, hi, (lo > hi) ? -((step < 0) ? -step : step) : step, COUNTER_MODE_WRAP);
    const uint32_t nbits = bits_needed(lo, hi);
    const uint32_t ndig  = (nbits + 3U) / 4U;

    for (uint32_t i = 0U; i < ARRAY_SIZE(leds); i++) {
        hal_gpio_mode(leds[i], HAL_GPIO_OUTPUT);
    }
    io_println("(q to stop)");
    do {
        const uint32_t v = (uint32_t)counter_get(&c);
        switch (show) {
        case SHOW_DEC:  counter_print_decimal(v, 3U);   break;
        case SHOW_HEX:  counter_print_hex(v, ndig);     break;
        case SHOW_BIN:  counter_print_binary(v, nbits); break;
        case SHOW_BCD:  counter_print_bcd(v, 3U);       break;
        case SHOW_GRAY: counter_print_gray(v, nbits);   break;
        case SHOW_LEDS:
        case SHOW_ALL:
        default:        counter_print_all(v, nbits);    break;
        }
        /* low bits on the LEDs: a hardware binary counter */
        for (uint32_t b = 0U; b < ARRAY_SIZE(leds); b++) {
            hal_gpio_write(leds[b], ((v >> b) & 1U) == (uint32_t)(BOARD_LED_ACTIVE_HIGH != 0));
        }
        (void)counter_step(&c);
        hal_delay_ms(period);
    } while (!cli_abort_requested());
    io_printf("stopped after %lu wrap(s)" IO_NEWLINE, (unsigned long)c.wraps);
    return CLI_OK;
}

static const cli_command_t counter_cmds[] = {
    { "count", cmd_count, "[lo hi step ms all|dec|hex|bin|bcd|gray|leds]", "TP2 counters", NULL },
};

static const cli_group_t counter_group = { "counters", counter_cmds, 1U };

const cli_group_t *cmd_counter_group(void)
{
    return &counter_group;
}

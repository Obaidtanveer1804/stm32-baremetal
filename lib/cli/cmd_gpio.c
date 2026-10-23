/**
 * @file    cmd_gpio.c
 * @brief   GPIO commands: gpio <sub>, led, button.
 *
 *   gpio mode PC8 out        gpio set PC8         gpio port C
 *   gpio toggle PC8          gpio read PA0        gpio port C 0x0300 0x0000
 *   gpio pulse PC8 500       gpio blink PC9 100 10
 *   gpio monitor PA0         gpio info
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "strutil.h"

static bool parse_pin(const char *s, hal_pin_t *pin)
{
    if (!hal_gpio_parse(s, pin)) {
        (void)cli_error("'%s' is not a pin (use PA0..PE15)", s);
        return false;
    }
    return true;
}

/* argv[0] is the sub-command, argv[1] the pin */
static int sub_mode(int argc, char *argv[])
{
    hal_pin_t       pin;
    hal_gpio_mode_t mode;
    if (argc != 3) {
        return CLI_ERR_USAGE;
    }
    if (!parse_pin(argv[1], &pin)) {
        return CLI_ERR;
    }
    if (!hal_gpio_parse_mode(argv[2], &mode)) {
        io_puts("modes:");
        for (uint32_t m = 0U; m < HAL_GPIO_MODE_COUNT; m++) {
            io_printf(" %s", hal_gpio_mode_name((hal_gpio_mode_t)m));
        }
        io_puts(IO_NEWLINE);
        return CLI_ERR;
    }
    hal_gpio_mode(pin, mode);
    return CLI_OK;
}

/* Output commands configure the pin as an output first if needed. */
static bool pin_for_output(int argc, char *argv[], hal_pin_t *pin)
{
    if ((argc < 2) || !parse_pin(argv[1], pin)) {
        return false;
    }
    const hal_gpio_mode_t m = hal_gpio_get_mode(*pin);
    if ((m != HAL_GPIO_OUTPUT) && (m != HAL_GPIO_OUTPUT_FAST) && (m != HAL_GPIO_OUTPUT_OD)) {
        hal_gpio_mode(*pin, HAL_GPIO_OUTPUT);
    }
    return true;
}

static int sub_set(int argc, char *argv[])
{
    hal_pin_t pin;
    if (!pin_for_output(argc, argv, &pin)) {
        return CLI_ERR_USAGE;
    }
    hal_gpio_set(pin);
    return CLI_OK;
}

static int sub_clr(int argc, char *argv[])
{
    hal_pin_t pin;
    if (!pin_for_output(argc, argv, &pin)) {
        return CLI_ERR_USAGE;
    }
    hal_gpio_clear(pin);
    return CLI_OK;
}

static int sub_toggle(int argc, char *argv[])
{
    hal_pin_t pin;
    if (!pin_for_output(argc, argv, &pin)) {
        return CLI_ERR_USAGE;
    }
    hal_gpio_toggle(pin);
    return CLI_OK;
}

static int sub_write(int argc, char *argv[])
{
    hal_pin_t pin;
    bool      level;
    if ((argc != 3) || !pin_for_output(argc, argv, &pin)) {
        return CLI_ERR_USAGE;
    }
    if (!cli_arg_bool(argv[2], &level)) {
        return CLI_ERR;
    }
    hal_gpio_write(pin, level);
    return CLI_OK;
}

static int sub_read(int argc, char *argv[])
{
    hal_pin_t pin;
    char      name[6];
    if ((argc != 2) || !parse_pin(argv[1], &pin)) {
        return CLI_ERR_USAGE;
    }
    io_printf("%s = %u (mode %s, odr %u)" IO_NEWLINE, hal_gpio_name(pin, name),
              hal_gpio_read(pin) ? 1U : 0U, hal_gpio_mode_name(hal_gpio_get_mode(pin)),
              hal_gpio_read_output(pin) ? 1U : 0U);
    return CLI_OK;
}

static bool parse_port(const char *s, hal_port_t *port)
{
    const char c = chr_to_upper((s[0] == 'P' || s[0] == 'p') ? s[1] : s[0]);
    if ((c < 'A') || (c >= (char)('A' + HAL_PORT_COUNT))) {
        (void)cli_error("'%s' is not a port (A..E)", s);
        return false;
    }
    *port = (hal_port_t)(c - 'A');
    return true;
}

static int sub_port(int argc, char *argv[])
{
    hal_port_t port;
    uint32_t   set_mask;
    uint32_t   clr_mask = 0U;
    if ((argc < 2) || !parse_port(argv[1], &port)) {
        return CLI_ERR_USAGE;
    }
    if (argc == 2) {
        const uint16_t idr = hal_gpio_port_read(port);
        const uint16_t odr = hal_gpio_port_read_output(port);
        io_printf("P%c  IDR 0x%04X  ODR 0x%04X" IO_NEWLINE, (char)('A' + port), idr, odr);
        io_puts("    pin 15..0 : ");
        io_print_bin(idr, 16U);
        io_puts(IO_NEWLINE);
        return CLI_OK;
    }
    if (!cli_arg_u32_range(argv[2], 0U, 0xFFFFU, &set_mask)) {
        return CLI_ERR;
    }
    if ((argc >= 4) && !cli_arg_u32_range(argv[3], 0U, 0xFFFFU, &clr_mask)) {
        return CLI_ERR;
    }
    /* BSRR: one atomic write for the whole port */
    hal_gpio_port_write(port, (uint16_t)set_mask, (uint16_t)clr_mask);
    return CLI_OK;
}

static int sub_pulse(int argc, char *argv[])
{
    hal_pin_t pin;
    uint32_t  us;
    if ((argc != 3) || !pin_for_output(argc, argv, &pin)) {
        return CLI_ERR_USAGE;
    }
    if (!cli_arg_u32_range(argv[2], 1U, 1000000U, &us)) {
        return CLI_ERR;
    }
    hal_gpio_toggle(pin);
    hal_delay_us(us);
    hal_gpio_toggle(pin);
    return CLI_OK;
}

static int sub_blink(int argc, char *argv[])
{
    hal_pin_t pin;
    uint32_t  half_ms;
    uint32_t  count;
    if ((argc != 4) || !pin_for_output(argc, argv, &pin)) {
        return CLI_ERR_USAGE;
    }
    if (!cli_arg_u32_range(argv[2], 1U, 10000U, &half_ms) ||
        !cli_arg_u32_range(argv[3], 1U, 100000U, &count)) {
        return CLI_ERR;
    }
    for (uint32_t i = 0U; i < (count * 2U); i++) {
        hal_gpio_toggle(pin);
        hal_delay_ms(half_ms);
        if (cli_abort_requested()) {
            break;
        }
    }
    return CLI_OK;
}

static int sub_monitor(int argc, char *argv[])
{
    hal_pin_t pin;
    char      name[6];
    if ((argc != 2) || !parse_pin(argv[1], &pin)) {
        return CLI_ERR_USAGE;
    }
    bool     last  = hal_gpio_read(pin);
    uint32_t t_last = hal_time_ms();
    io_printf("%s = %u, watching edges (q to stop)" IO_NEWLINE, hal_gpio_name(pin, name), last ? 1U : 0U);
    while (!cli_abort_requested()) {
        const bool now = hal_gpio_read(pin);
        if (now != last) {
            const uint32_t t = hal_time_ms();
            io_printf("[%8lu ms] %s %s after %lu ms" IO_NEWLINE, (unsigned long)t, name,
                      now ? "rising " : "falling", (unsigned long)(t - t_last));
            last   = now;
            t_last = t;
        }
    }
    return CLI_OK;
}

static int sub_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    io_println("pin   mode      in out");
    for (uint32_t port = 0U; port < HAL_PORT_COUNT; port++) {
        for (uint32_t n = 0U; n < 16U; n++) {
            const hal_pin_t       pin = HAL_PIN(port, n);
            const hal_gpio_mode_t m   = hal_gpio_get_mode(pin);
            if (m != HAL_GPIO_INPUT) {
                char name[6];
                io_printf("%-5s %-9s %u  %u" IO_NEWLINE, hal_gpio_name(pin, name), hal_gpio_mode_name(m),
                          hal_gpio_read(pin) ? 1U : 0U, hal_gpio_read_output(pin) ? 1U : 0U);
            }
        }
    }
    return CLI_OK;
}

static const cli_subcmd_t gpio_subs[] = {
    { "mode",    sub_mode,    "<pin> <in|pullup|pulldown|analog|out|outfast|od|af|afod>", "configure a pin" },
    { "set",     sub_set,     "<pin>",            "drive high (makes it an output)" },
    { "clr",     sub_clr,     "<pin>",            "drive low" },
    { "toggle",  sub_toggle,  "<pin>",            "invert the output" },
    { "write",   sub_write,   "<pin> <0|1>",      "drive a level" },
    { "read",    sub_read,    "<pin>",            "input level + mode" },
    { "port",    sub_port,    "<A-E> [set] [clr]", "read a port, or BSRR write masks" },
    { "pulse",   sub_pulse,   "<pin> <us>",       "one pulse of a given width" },
    { "blink",   sub_blink,   "<pin> <ms> <n>",   "toggle n times" },
    { "monitor", sub_monitor, "<pin>",            "print edges with timing" },
    { "info",    sub_info,    "",                 "list configured pins" },
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------- led ------------------------------ */

static const hal_pin_t board_leds[] = BOARD_LEDS_INIT;
#define LED_COUNT  (sizeof(board_leds) / sizeof(board_leds[0]))

static void led_apply(uint32_t index, const char *action)
{
    const hal_pin_t pin = board_leds[index];
    const bool      on_level = (BOARD_LED_ACTIVE_HIGH != 0);
    hal_gpio_mode(pin, HAL_GPIO_OUTPUT);
    if (str_eq(action, "on")) {
        hal_gpio_write(pin, on_level);
    } else if (str_eq(action, "off")) {
        hal_gpio_write(pin, !on_level);
    } else {
        hal_gpio_toggle(pin);
    }
}

static int cmd_led(int argc, char *argv[])
{
    uint32_t index;
    if ((argc != 3) || (!str_eq(argv[2], "on") && !str_eq(argv[2], "off") && !str_eq(argv[2], "toggle"))) {
        return CLI_ERR_USAGE;
    }
    if (str_eq(argv[1], "all")) {
        for (uint32_t i = 0U; i < LED_COUNT; i++) {
            led_apply(i, argv[2]);
        }
        return CLI_OK;
    }
    if (str_eq(argv[1], "blue")) {
        index = 0U;
    } else if (str_eq(argv[1], "green")) {
        index = LED_COUNT - 1U;
    } else if (!cli_arg_u32_range(argv[1], 0U, LED_COUNT - 1U, &index)) {
        return CLI_ERR;
    } else {
        /* numeric index */
    }
    led_apply(index, argv[2]);
    return CLI_OK;
}

static int cmd_button(int argc, char *argv[])
{
    const bool active_high = (BOARD_BUTTON_ACTIVE_HIGH != 0);
    hal_gpio_mode(BOARD_BUTTON, active_high ? HAL_GPIO_INPUT_PULLDOWN : HAL_GPIO_INPUT_PULLUP);
    if ((argc == 2) && str_eq(argv[1], "wait")) {
        io_println("press the button (q to cancel)");
        while (hal_gpio_read(BOARD_BUTTON) != active_high) {
            if (cli_abort_requested()) {
                return CLI_OK;
            }
        }
        io_println("pressed");
        return CLI_OK;
    }
    io_printf("button %s" IO_NEWLINE, (hal_gpio_read(BOARD_BUTTON) == active_high) ? "pressed" : "released");
    return CLI_OK;
}

static const cli_command_t gpio_cmds[] = {
    { "gpio",   NULL,       "<sub> ...",                  "pin control", gpio_subs },
    { "led",    cmd_led,    "<0|1|blue|green|all> <on|off|toggle>", "board LEDs", NULL },
    { "button", cmd_button, "[wait]",                     "read the USER button", NULL },
};

static const cli_group_t gpio_group = { "gpio", gpio_cmds, sizeof(gpio_cmds) / sizeof(gpio_cmds[0]) };

const cli_group_t *cmd_gpio_group(void)
{
    return &gpio_group;
}

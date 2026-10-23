/**
 * @file    cmd_sim.c
 * @brief   "sim" command: drive the PC simulator from the shell.
 *          Compiled only for the PC (BOARD_PC).
 *
 *   sim status                 pins, PWM, timers, ADC sources
 *   sim log gpio on            trace GPIO changes (gpio pwm adc spi i2c uart timer flash exti sys all)
 *   sim pin PA0 1              drive an input (press the USER button)
 *   sim press PA0 [ms]         press and release
 *   sim release PA0            stop driving it
 *   sim adc 1 2048             constant voltage on channel 1
 *   sim wave 1 sine 2048 1000 500    sine around 2048, +-1000, 500 ms period
 *   sim noise 8                +-8 LSB noise on every conversion
 *   sim temp 31.5              temperature seen by LM75 / MCU sensor
 *   sim leds on                live LED bar in the corner
 *   sim devices                attached chip models
 *   sim uart 2 loop on         TX->RX jumper on UART2
 *   sim oled                   draw the simulated SSD1306 screen
 */
#ifdef BOARD_PC

#include "cli_cmds.h"
#include "sim.h"
#include "io.h"
#include "strutil.h"

static int sub_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    sim_status();
    return CLI_OK;
}

static int sub_log(int argc, char *argv[])
{
    uint32_t mask;
    bool     on = true;
    if (argc == 1) {
        io_puts("logging:");
        for (uint32_t b = 0U; b < 10U; b++) {
            if ((sim_get_log() & (1U << b)) != 0U) {
                io_printf(" %s", sim_log_name(1U << b));
            }
        }
        io_puts(IO_NEWLINE);
        return CLI_OK;
    }
    if (!sim_parse_log(argv[1], &mask)) {
        return cli_error("categories: gpio pwm adc spi i2c uart timer flash exti sys all none");
    }
    if ((argc >= 3) && !cli_arg_bool(argv[2], &on)) {
        return CLI_ERR;
    }
    sim_set_log(on ? (sim_get_log() | mask) : (sim_get_log() & ~mask));
    if (mask == 0U) {
        sim_set_log(0U);
    }
    return CLI_OK;
}

static bool pin_arg(const char *s, hal_pin_t *pin)
{
    if (!hal_gpio_parse(s, pin)) {
        (void)cli_error("bad pin '%s'", s);
        return false;
    }
    return true;
}

static int sub_pin(int argc, char *argv[])
{
    hal_pin_t pin;
    bool      level;
    if ((argc != 3) || !pin_arg(argv[1], &pin) || !cli_arg_bool(argv[2], &level)) {
        return CLI_ERR_USAGE;
    }
    sim_gpio_input(pin, level);
    return CLI_OK;
}

static int sub_press(int argc, char *argv[])
{
    hal_pin_t pin;
    uint32_t  ms = 100U;
    if ((argc < 2) || !pin_arg(argv[1], &pin)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 1U, 60000U, &ms)) {
        return CLI_ERR;
    }
    const bool active_high = (BOARD_BUTTON_ACTIVE_HIGH != 0) || (pin != BOARD_BUTTON);
    sim_gpio_input(pin, active_high);
    hal_delay_ms(ms);
    sim_gpio_input(pin, !active_high);
    return CLI_OK;
}

static int sub_release(int argc, char *argv[])
{
    hal_pin_t pin;
    if ((argc != 2) || !pin_arg(argv[1], &pin)) {
        return CLI_ERR_USAGE;
    }
    sim_gpio_release(pin);
    return CLI_OK;
}

static int sub_adc(int argc, char *argv[])
{
    uint32_t ch;
    uint32_t raw;
    if ((argc != 3) || !cli_arg_u32_range(argv[1], 0U, HAL_ADC_CH_COUNT - 1U, &ch) ||
        !cli_arg_u32_range(argv[2], 0U, HAL_ADC_MAX, &raw)) {
        return CLI_ERR_USAGE;
    }
    sim_adc_set((uint8_t)ch, (uint16_t)raw);
    return CLI_OK;
}

static int sub_wave(int argc, char *argv[])
{
    uint32_t   ch;
    uint32_t   center;
    uint32_t   amp;
    uint32_t   period;
    sim_wave_t wave;
    if ((argc != 6) || !cli_arg_u32_range(argv[1], 0U, HAL_ADC_CH_COUNT - 1U, &ch) ||
        !cli_arg_u32_range(argv[3], 0U, HAL_ADC_MAX, &center) ||
        !cli_arg_u32_range(argv[4], 0U, HAL_ADC_MAX, &amp) ||
        !cli_arg_u32_range(argv[5], 1U, 3600000U, &period)) {
        return CLI_ERR_USAGE;
    }
    if (!sim_parse_wave(argv[2], &wave)) {
        return cli_error("waves: const sine triangle square ramp noise");
    }
    sim_adc_wave((uint8_t)ch, wave, (uint16_t)center, (uint16_t)amp, period);
    return CLI_OK;
}

static int sub_noise(int argc, char *argv[])
{
    uint32_t lsb;
    if ((argc != 2) || !cli_arg_u32_range(argv[1], 0U, 1000U, &lsb)) {
        return CLI_ERR_USAGE;
    }
    sim_adc_noise((uint16_t)lsb);
    return CLI_OK;
}

static int sub_temp(int argc, char *argv[])
{
    int32_t c10;
    if ((argc != 2) || !str_to_fixed(argv[1], 10U, &c10)) {
        return CLI_ERR_USAGE;
    }
    sim_set_temperature_c10(c10);
    return CLI_OK;
}

static int sub_leds(int argc, char *argv[])
{
    bool on;
    if ((argc != 2) || !cli_arg_bool(argv[1], &on)) {
        return CLI_ERR_USAGE;
    }
    sim_gpio_show_leds(on);
    return CLI_OK;
}

static int sub_devices(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    sim_devices_list();
    return CLI_OK;
}

static int sub_oled(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    sim_oled_show();
    return CLI_OK;
}

static int sub_uart(int argc, char *argv[])
{
    uint32_t u;
    bool     on;
    if ((argc != 4) || !cli_arg_u32_range(argv[1], 1U, HAL_UART_COUNT, &u) || !str_eq(argv[2], "loop") ||
        !cli_arg_bool(argv[3], &on)) {
        return CLI_ERR_USAGE;
    }
    sim_uart_loopback((hal_uart_t)(u - 1U), on);
    return CLI_OK;
}

static const cli_subcmd_t sim_subs[] = {
    { "status",  sub_status,  "",                               "simulator state" },
    { "log",     sub_log,     "[category] [on|off]",            "trace hardware activity" },
    { "pin",     sub_pin,     "<pin> <0|1>",                    "drive an input pin" },
    { "press",   sub_press,   "<pin> [ms]",                     "press + release a button" },
    { "release", sub_release, "<pin>",                          "stop driving a pin" },
    { "adc",     sub_adc,     "<ch> <0..4095>",                 "constant ADC input" },
    { "wave",    sub_wave,    "<ch> <wave> <center> <amp> <ms>","ADC signal generator" },
    { "noise",   sub_noise,   "<lsb>",                          "ADC noise amplitude" },
    { "temp",    sub_temp,    "<celsius>",                      "simulated temperature" },
    { "leds",    sub_leds,    "<on|off>",                       "live LED display" },
    { "devices", sub_devices, "",                               "chip models on the buses" },
    { "oled",    sub_oled,    "",                               "show the simulated OLED" },
    { "uart",    sub_uart,    "<n> loop <on|off>",              "UART loopback jumper" },
    { NULL, NULL, NULL, NULL }
};

static const cli_command_t sim_cmds[] = {
    { "sim", NULL, "<sub> ...", "PC simulator controls", sim_subs },
};

static const cli_group_t sim_group = { "simulator", sim_cmds, 1U };

const cli_group_t *cmd_sim_group(void)
{
    return &sim_group;
}

#endif /* BOARD_PC */

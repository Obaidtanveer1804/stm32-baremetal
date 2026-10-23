/**
 * @file    cmd_rtc.c
 * @brief   Calendar commands on the RTC seconds counter.
 *
 *   date                       2026-10-16 08:30:12 Fri
 *   date 2026-10-16 08:30:00   set
 *   date 12:00                 set the time only
 *   alarm 10                   LED on in 10 s (RTC alarm interrupt)
 *   stopwatch                  Enter = lap, q = stop
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "datetime.h"
#include "strutil.h"

static void print_now(void)
{
    datetime_t dt;
    char       buf[24];
    dt_from_unix(hal_rtc_get_seconds(), &dt);
    (void)dt_format(&dt, buf, sizeof(buf));
    io_printf("%s %s" IO_NEWLINE, buf, dt_weekday_name(dt.weekday));
}

static int cmd_date(int argc, char *argv[])
{
    datetime_t dt;
    char       text[24] = "";
    uint32_t   t;

    (void)hal_rtc_init();
    if (argc == 1) {
        print_now();
        return CLI_OK;
    }
    /* "date 2026-10-16 08:30" arrives as two words: join them */
    for (int i = 1; (i < argc) && (i < 3); i++) {
        (void)str_cat(text, argv[i], sizeof(text));
        if (i < (argc - 1)) {
            (void)str_cat(text, " ", sizeof(text));
        }
    }
    dt_from_unix(hal_rtc_get_seconds(), &dt);
    if (!dt_parse(text, &dt) || !dt_to_unix(&dt, &t)) {
        return cli_error("format: YYYY-MM-DD [HH:MM[:SS]] or HH:MM[:SS]");
    }
    hal_rtc_set_seconds(t);
    print_now();
    return CLI_OK;
}

static void alarm_cb(void *arg)
{
    (void)arg;
    hal_gpio_mode(BOARD_LED_GREEN, HAL_GPIO_OUTPUT);
    hal_gpio_write(BOARD_LED_GREEN, BOARD_LED_ACTIVE_HIGH != 0);
    io_puts(IO_NEWLINE "*** RTC ALARM ***" IO_NEWLINE);
}

static int cmd_alarm(int argc, char *argv[])
{
    uint32_t s;
    if (argc != 2) {
        return CLI_ERR_USAGE;
    }
    if (str_eq(argv[1], "off")) {
        hal_rtc_cancel_alarm();
        return CLI_OK;
    }
    if (!cli_arg_u32_range(argv[1], 1U, 86400U * 7U, &s)) {
        return CLI_ERR;
    }
    (void)hal_rtc_init();
    (void)hal_rtc_set_alarm(hal_rtc_get_seconds() + s, alarm_cb, NULL);
    io_printf("alarm in %lu s (green LED + message)" IO_NEWLINE, (unsigned long)s);
    return CLI_OK;
}

static int cmd_stopwatch(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    io_println("Enter = lap, q = stop");
    const uint32_t t0   = hal_time_ms();
    uint32_t       lap0 = t0;
    uint32_t       laps = 0U;
    for (;;) {
        const int c = io_getc();
        const uint32_t now = hal_time_ms();
        if ((c == 'q') || (c == 0x03) || (c == IO_CLOSED)) {
            break;
        }
        if ((c == '\r') || (c == '\n')) {
            laps++;
            io_printf(IO_NEWLINE "lap %lu: %lu.%03lu s" IO_NEWLINE, (unsigned long)laps,
                      (unsigned long)((now - lap0) / 1000U), (unsigned long)((now - lap0) % 1000U));
            lap0 = now;
        }
        io_printf("\r%5lu.%02lu s ", (unsigned long)((now - t0) / 1000U),
                  (unsigned long)(((now - t0) % 1000U) / 10U));
        hal_delay_ms(20U);
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

static const cli_command_t rtc_cmds[] = {
    { "date",      cmd_date,      "[YYYY-MM-DD] [HH:MM[:SS]]", "show / set the RTC calendar", NULL },
    { "alarm",     cmd_alarm,     "<seconds|off>",             "RTC alarm interrupt",         NULL },
    { "stopwatch", cmd_stopwatch, "",                          "lap timer on SysTick",        NULL },
};

static const cli_group_t rtc_group = { "clock", rtc_cmds, sizeof(rtc_cmds) / sizeof(rtc_cmds[0]) };

const cli_group_t *cmd_rtc_group(void)
{
    return &rtc_group;
}

/**
 * @file    cmd_sys.c
 * @brief   System commands: info, uptime, reset, echo, delay, log, top,
 *          wdg, irq-latency style timing ("time <cmd>").
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "log.h"
#include "sched.h"
#include "datetime.h"
#include "strutil.h"
#include "crc.h"
#include "fixmath.h"
#include "fmt.h"
#include "dsp.h"

#ifndef APP_VERSION
#define APP_VERSION  "1.0"
#endif

static void print_reset_cause(uint32_t cause)
{
    static const char *const names[] = { "power-on", "pin", "software", "iwdg", "wwdg", "low-power" };
    bool any = false;
    for (uint32_t i = 0U; i < (sizeof(names) / sizeof(names[0])); i++) {
        if ((cause & (1U << i)) != 0U) {
            io_printf("%s%s", any ? "+" : "", names[i]);
            any = true;
        }
    }
    if (!any) {
        io_puts("unknown");
    }
}

static int cmd_info(int argc, char *argv[])
{
    uint32_t id[3];
    (void)argc;
    (void)argv;
    hal_sys_unique_id(id);
    io_printf("board     : %s" IO_NEWLINE, BOARD_NAME);
    io_printf("chip      : %s" IO_NEWLINE, hal_sys_name());
    io_printf("clock     : %lu Hz" IO_NEWLINE, (unsigned long)hal_sys_clock_hz());
    io_printf("flash/ram : %lu KiB / %lu KiB" IO_NEWLINE,
              (unsigned long)hal_sys_flash_size_kb(), (unsigned long)hal_sys_ram_size_kb());
    io_printf("unique id : %08lX-%08lX-%08lX" IO_NEWLINE,
              (unsigned long)id[2], (unsigned long)id[1], (unsigned long)id[0]);
    io_puts("reset     : ");
    print_reset_cause(hal_sys_reset_cause());
    io_puts(IO_NEWLINE);
    io_printf("stack free: %lu bytes (never used since boot)" IO_NEWLINE,
              (unsigned long)hal_sys_stack_free());
    io_printf("firmware  : v" APP_VERSION " built " __DATE__ " " __TIME__ IO_NEWLINE);
    return CLI_OK;
}

static int cmd_uptime(int argc, char *argv[])
{
    char buf[24];
    (void)argc;
    (void)argv;
    const uint32_t ms = hal_time_ms();
    (void)dt_format_duration(ms / 1000U, buf, sizeof(buf));
    io_printf("up %s.%03lu (%lu ms)" IO_NEWLINE, buf, (unsigned long)(ms % 1000U), (unsigned long)ms);
    return CLI_OK;
}

static int cmd_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    io_println("resetting...");
    hal_delay_ms(10U);          /* let the UART finish */
    hal_sys_reset();
}

static int cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        io_printf("%s%s", argv[i], (i < (argc - 1)) ? " " : "");
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

static int cmd_delay(int argc, char *argv[])
{
    uint32_t ms;
    if (argc != 2) {
        return CLI_ERR_USAGE;
    }
    if (!cli_arg_u32_range(argv[1], 0U, 600000U, &ms)) {
        return CLI_ERR;
    }
    hal_delay_ms(ms);
    return CLI_OK;
}

static int cmd_log(int argc, char *argv[])
{
    int32_t level;
    if (argc == 1) {
        io_printf("log level: %s" IO_NEWLINE, log_level_name(log_get_level()));
        return CLI_OK;
    }
    if (!log_parse_level(argv[1], &level)) {
        return cli_error("levels: none error warn info debug trace");
    }
    log_set_level(level);
    if (argc >= 3) {
        bool color;
        if (!cli_arg_bool(argv[2], &color)) {
            return CLI_ERR;
        }
        log_set_color(color);
    }
    return CLI_OK;
}

static int cmd_top(int argc, char *argv[])
{
    if ((argc == 2) && str_eq(argv[1], "reset")) {
        sched_reset_stats();
        return CLI_OK;
    }
    if (sched_task_count() == 0U) {
        io_println("no scheduler tasks (the app does not use sched.h)");
        return CLI_OK;
    }
    io_println("  #  task          period    runs      max us   avg us  on");
    for (uint32_t i = 0U; i < sched_task_count(); i++) {
        const sched_task_t *t   = sched_task_get(i);
        const uint32_t      avg = (t->runs != 0U) ? (t->total_us / t->runs) : 0U;
        io_printf("%3lu  %-12s %6lu ms %8lu %8lu %8lu  %s" IO_NEWLINE, (unsigned long)i, t->name,
                  (unsigned long)t->period_ms, (unsigned long)t->runs, (unsigned long)t->max_us,
                  (unsigned long)avg, t->enabled ? "yes" : "no");
    }
    return CLI_OK;
}

static int cmd_task(int argc, char *argv[])
{
    uint32_t period;
    if (argc < 3) {
        return CLI_ERR_USAGE;
    }
    const int32_t id = sched_find(argv[1]);
    if (id < 0) {
        return cli_error("no task '%s' (see 'top')", argv[1]);
    }
    if (str_eq(argv[2], "on") || str_eq(argv[2], "off")) {
        sched_enable(id, str_eq(argv[2], "on"));
        return CLI_OK;
    }
    if (!cli_arg_u32_range(argv[2], 1U, 3600000U, &period)) {
        return CLI_ERR;
    }
    sched_set_period(id, period);
    return CLI_OK;
}

static int cmd_wdg(int argc, char *argv[])
{
    uint32_t ms;
    if (argc == 1) {
        io_printf("watchdog %s, timeout %lu ms" IO_NEWLINE,
                  hal_wdg_is_running() ? "running" : "off", (unsigned long)hal_wdg_timeout_ms());
        return CLI_OK;
    }
    if (str_eq(argv[1], "kick")) {
        hal_wdg_kick();
        return CLI_OK;
    }
    if (str_eq(argv[1], "starve")) {
        io_println("not kicking the watchdog any more: the chip will reset");
        for (;;) {
            /* main loop blocked on purpose */
        }
    }
    if (!str_eq(argv[1], "start") || (argc < 3) || !cli_arg_u32(argv[2], &ms)) {
        return CLI_ERR_USAGE;
    }
    const hal_status_t st = hal_wdg_start(ms);
    return (st == HAL_OK) ? CLI_OK : cli_error("%s", hal_status_str(st));
}

static int cmd_time(int argc, char *argv[])
{
    char line[CLI_LINE_MAX] = "";
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    for (int i = 1; i < argc; i++) {
        (void)str_cat(line, argv[i], sizeof(line));
        (void)str_cat(line, " ", sizeof(line));
    }
    const uint32_t t0 = hal_time_us();
    const int      rc = cli_execute(line);
    const uint32_t dt = hal_time_us() - t0;
    io_printf("took %lu us" IO_NEWLINE, (unsigned long)dt);
    return rc;
}


/* ------------------------------ bench ----------------------------- */

#define BENCH_BUF_SIZE  (1024U)   /* + 512 B of FFT data = 1.5 KiB scratch */
#define BENCH_FFT_LOG2  (7U)

static volatile uint32_t s_bench_sink;    /* keeps results "used" */

typedef struct {
    const char *name;
    void      (*fn)(void);
} bench_t;

/* both live in the CLI scratch area while "bench" runs */
static uint8_t    *s_bench_buf;
static dsp_cplx_t *s_bench_fft;

static void b_crc32(void)    { s_bench_sink = crc32(s_bench_buf, BENCH_BUF_SIZE); }
static void b_crc16(void)    { s_bench_sink = crc16_ccitt(CRC16_CCITT_INIT, s_bench_buf, BENCH_BUF_SIZE); }
static void b_isqrt(void)    { s_bench_sink = fx_isqrt32(s_bench_sink | 0x12345678U); }
static void b_sin(void)      { s_bench_sink = (uint32_t)q16_sin((uint16_t)s_bench_sink); }
static void b_sin_tab(void)  { s_bench_sink = (uint32_t)fx_sin_q15((uint16_t)s_bench_sink); }
static void b_div(void)      { s_bench_sink = (s_bench_sink | 1U) / 7U; }
static void b_div64(void)    { s_bench_sink = (uint32_t)(((uint64_t)s_bench_sink << 20) / 12345U); }
static void b_printf(void)
{
    char buf[32];
    s_bench_sink = (uint32_t)fmt_snprintf(buf, sizeof(buf), "%lu %x", (unsigned long)s_bench_sink, 0xBEEFU);
}
static void b_fft(void)
{
    for (uint32_t i = 0U; i < (1U << BENCH_FFT_LOG2); i++) {
        s_bench_fft[i].re = (int16_t)(i * 97U);
        s_bench_fft[i].im = 0;
    }
    (void)dsp_fft(s_bench_fft, BENCH_FFT_LOG2);
}
static void b_gpio(void)
{
    for (uint32_t i = 0U; i < 100U; i++) {
        hal_gpio_toggle(BOARD_LED_BLUE);
    }
}

static int cmd_bench(int argc, char *argv[])
{
    static const bench_t benches[] = {
        { "crc32 1 KiB",        b_crc32 },
        { "crc16 1 KiB",        b_crc16 },
        { "fft 128 pts Q15",    b_fft },
        { "isqrt32",            b_isqrt },
        { "sin CORDIC Q16",     b_sin },
        { "sin table Q15",      b_sin_tab },
        { "u32 / 7",            b_div },
        { "u64 / u32",          b_div64 },
        { "snprintf 2 fields",  b_printf },
        { "gpio toggle x100",   b_gpio },
    };
    (void)argc;
    (void)argv;
    uint8_t *scratch = (uint8_t *)cli_scratch(BENCH_BUF_SIZE + (sizeof(dsp_cplx_t) << BENCH_FFT_LOG2));
    if (scratch == NULL) {
        return cli_error("scratch area too small");
    }
    s_bench_buf = scratch;
    s_bench_fft = (dsp_cplx_t *)(void *)&scratch[BENCH_BUF_SIZE];
    hal_sys_cycles_init();
    hal_gpio_mode(BOARD_LED_BLUE, HAL_GPIO_OUTPUT);
    for (uint32_t i = 0U; i < BENCH_BUF_SIZE; i++) {
        s_bench_buf[i] = (uint8_t)(i * 31U);
    }
    const uint32_t mhz = hal_sys_clock_hz() / 1000000U;
    io_printf("cycles at %lu MHz (best of 8 runs, interrupts on)" IO_NEWLINE, (unsigned long)mhz);
#ifdef BOARD_PC
    io_println("(PC simulation: host speed, run it on the board for real numbers)");
#endif
    for (uint32_t b = 0U; b < (sizeof(benches) / sizeof(benches[0])); b++) {
        uint32_t best = 0xFFFFFFFFU;
        for (uint32_t run = 0U; run < 8U; run++) {
            const uint32_t c0 = hal_sys_cycles();
            benches[b].fn();
            const uint32_t dc = hal_sys_cycles() - c0;
            best = (dc < best) ? dc : best;
        }
        io_printf("  %-20s %9lu cycles %9lu us" IO_NEWLINE, benches[b].name, (unsigned long)best,
                  (unsigned long)((mhz == 0U) ? 0U : (best / mhz)));
    }
    return CLI_OK;
}

static const cli_command_t sys_cmds[] = {
    { "info",   cmd_info,   "",                   "board, chip, clock, IDs, reset cause", NULL },
    { "uptime", cmd_uptime, "",                   "time since boot",                      NULL },
    { "reset",  cmd_reset,  "",                   "software reset (NVIC SYSRESETREQ)",    NULL },
    { "echo",   cmd_echo,   "<text...>",          "print the arguments",                  NULL },
    { "delay",  cmd_delay,  "<ms>",               "wait (useful in ';' chains)",          NULL },
    { "log",    cmd_log,    "[level] [color]",    "show / set the log level",             NULL },
    { "top",    cmd_top,    "[reset]",            "scheduler tasks: runs, worst time",    NULL },
    { "task",   cmd_task,   "<name> <ms|on|off>", "change a scheduler task",              NULL },
    { "wdg",    cmd_wdg,    "[start <ms>|kick|starve]", "independent watchdog",           NULL },
    { "time",   cmd_time,   "<cmd...>",           "measure how long a command takes",     NULL },
    { "bench",  cmd_bench,  "",                   "CPU cycles of common routines (DWT)",  NULL },
};

static const cli_group_t sys_group = { "system", sys_cmds, sizeof(sys_cmds) / sizeof(sys_cmds[0]) };

const cli_group_t *cmd_sys_group(void)
{
    return &sys_group;
}

/**
 * @file    cmd_timer.c
 * @brief   Timer and PWM commands: periodic timer that toggles a pin,
 *          PWM duty / frequency, servo pulses, LED breathing (fade).
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "fixmath.h"
#include "strutil.h"

static bool parse_timer(const char *s, hal_timer_t *t)
{
    uint32_t n;
    if (((s[0] == 't') || (s[0] == 'T')) && (s[1] != '\0')) {
        s = &s[(str_starts_with(s, "tim") || str_starts_with(s, "TIM")) ? 3 : 1];
    }
    if (!cli_arg_u32_range(s, 2U, 4U, &n)) {
        return false;
    }
    *t = (hal_timer_t)(n - 2U);
    return true;
}

/* "t3c3", "T4C1" or an index 0..7 */
static bool parse_pwm(const char *s, hal_pwm_t *ch)
{
    uint32_t n;
    if (((s[0] == 't') || (s[0] == 'T')) && chr_is_digit(s[1]) &&
        ((s[2] == 'c') || (s[2] == 'C')) && chr_is_digit(s[3]) && (s[4] == '\0')) {
        const uint32_t tim = (uint32_t)(s[1] - '0');
        const uint32_t c   = (uint32_t)(s[3] - '0');
        if ((tim >= 3U) && (tim <= 4U) && (c >= 1U) && (c <= 4U)) {
            *ch = (hal_pwm_t)(((tim - 3U) * 4U) + (c - 1U));
            return true;
        }
    }
    if (str_to_u32(s, &n) && (n < HAL_PWM_COUNT)) {
        *ch = (hal_pwm_t)n;
        return true;
    }
    (void)cli_error("'%s' is not a PWM channel (t3c1..t4c4 or 0..7)", s);
    return false;
}

/* --------------------------- timer -------------------------------- */

static hal_pin_t s_toggle_pin[HAL_TIMER_COUNT];

static void toggle_isr(hal_timer_t t, void *arg)
{
    (void)arg;
    hal_gpio_toggle(s_toggle_pin[t]);
}

static int sub_tstart(int argc, char *argv[])
{
    hal_timer_t t;
    uint32_t    hz;
    hal_pin_t   pin = HAL_PIN_NONE;
    if ((argc < 3) || !parse_timer(argv[1], &t) || !cli_arg_u32_range(argv[2], 1U, 1000000U, &hz)) {
        return CLI_ERR_USAGE;
    }
    if (argc >= 4) {
        if (!hal_gpio_parse(argv[3], &pin)) {
            return cli_error("bad pin '%s'", argv[3]);
        }
        hal_gpio_mode(pin, HAL_GPIO_OUTPUT_FAST);
    }
    s_toggle_pin[t] = pin;
    const hal_status_t st = hal_timer_start_periodic(t, hz, (pin != HAL_PIN_NONE) ? toggle_isr : NULL, NULL);
    if (st != HAL_OK) {
        return cli_error("%s", hal_status_str(st));
    }
    io_printf("TIM%u running at %lu Hz%s" IO_NEWLINE, (unsigned)t + 2U,
              (unsigned long)hal_timer_get_frequency(t), (pin != HAL_PIN_NONE) ? ", toggling the pin" : "");
    return CLI_OK;
}

static int sub_tstop(int argc, char *argv[])
{
    hal_timer_t t;
    if ((argc != 2) || !parse_timer(argv[1], &t)) {
        return CLI_ERR_USAGE;
    }
    hal_timer_stop(t);
    return CLI_OK;
}

static int sub_tinfo(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    for (uint32_t i = 0U; i < HAL_TIMER_COUNT; i++) {
        const hal_timer_t t = (hal_timer_t)i;
        io_printf("TIM%lu %-7s %7lu Hz  ticks %-10lu cnt %u" IO_NEWLINE, (unsigned long)i + 2U,
                  hal_timer_is_running(t) ? "running" : "stopped", (unsigned long)hal_timer_get_frequency(t),
                  (unsigned long)hal_timer_get_ticks(t), (unsigned)hal_timer_get_counter(t));
    }
    return CLI_OK;
}

static int sub_tcalc(int argc, char *argv[])
{
    uint32_t hz;
    uint16_t psc;
    uint16_t arr;
    if ((argc != 2) || !cli_arg_u32(argv[1], &hz)) {
        return CLI_ERR_USAGE;
    }
    const uint32_t clk = hal_timer_clock_hz(HAL_TIMER2);
    if (!hal_timer_calc(clk, hz, &psc, &arr)) {
        return cli_error("%lu Hz is out of range for a %lu Hz timer clock", (unsigned long)hz, (unsigned long)clk);
    }
    const uint32_t real = clk / (((uint32_t)psc + 1U) * ((uint32_t)arr + 1U));
    io_printf("clk %lu Hz -> PSC=%u ARR=%u -> %lu Hz (error %ld Hz)" IO_NEWLINE, (unsigned long)clk,
              (unsigned)psc, (unsigned)arr, (unsigned long)real, (long)((int32_t)real - (int32_t)hz));
    return CLI_OK;
}

static const cli_subcmd_t timer_subs[] = {
    { "start", sub_tstart, "<2|3|4> <hz> [pin]", "periodic interrupt, optional pin toggle" },
    { "stop",  sub_tstop,  "<2|3|4>",            "stop a timer" },
    { "info",  sub_tinfo,  "",                   "state of all timers" },
    { "calc",  sub_tcalc,  "<hz>",               "show PSC/ARR for a frequency" },
    { NULL, NULL, NULL, NULL }
};

/* ---------------------------- pwm --------------------------------- */

static int sub_pinit(int argc, char *argv[])
{
    hal_pwm_t ch;
    uint32_t  hz;
    if ((argc != 3) || !parse_pwm(argv[1], &ch) || !cli_arg_u32_range(argv[2], 1U, 1000000U, &hz)) {
        return CLI_ERR_USAGE;
    }
    const hal_status_t st = hal_pwm_init(ch, hz);
    if (st != HAL_OK) {
        return cli_error("%s", hal_status_str(st));
    }
    char name[6];
    io_printf("PWM on %s at %lu Hz" IO_NEWLINE, hal_gpio_name(hal_pwm_pin(ch), name),
              (unsigned long)hal_pwm_get_frequency(ch));
    return CLI_OK;
}

static int sub_pduty(int argc, char *argv[])
{
    hal_pwm_t ch;
    int32_t   pct10;
    if ((argc != 3) || !parse_pwm(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    /* accepts "25" or "12.5" percent */
    if (!str_to_fixed(argv[2], 10U, &pct10) || (pct10 < 0) || (pct10 > 1000)) {
        return cli_error("duty is a percentage 0..100 (decimals allowed)");
    }
    hal_pwm_set_duty(ch, (uint32_t)pct10);
    return CLI_OK;
}

static int sub_pstop(int argc, char *argv[])
{
    hal_pwm_t ch;
    if ((argc != 2) || !parse_pwm(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    hal_pwm_stop(ch);
    return CLI_OK;
}

static int sub_pfade(int argc, char *argv[])
{
    hal_pwm_t ch;
    uint32_t  period_ms = 2000U;
    if ((argc < 2) || !parse_pwm(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 100U, 60000U, &period_ms)) {
        return CLI_ERR;
    }
    if (hal_pwm_get_frequency(ch) == 0U) {
        (void)hal_pwm_init(ch, 1000U);
    }
    io_println("breathing LED, q to stop");
    const uint32_t t0 = hal_time_ms();
    while (!cli_abort_requested()) {
        const uint32_t phase = (((hal_time_ms() - t0) % period_ms) * 65536U) / period_ms;
        /* sine 0..1, squared: the eye sees brightness ~ log, this looks linear */
        const int32_t  s  = (fx_sin_q15((uint16_t)phase) + 32767) / 2;        /* 0..32767 */
        const uint32_t pm = (uint32_t)(((int64_t)s * s * 1000) / ((int64_t)32767 * 32767));
        hal_pwm_set_duty(ch, pm);
        hal_delay_ms(10U);
    }
    return CLI_OK;
}

static const cli_subcmd_t pwm_subs[] = {
    { "init", sub_pinit, "<t3c1..t4c4> <hz>",   "start PWM on a channel" },
    { "duty", sub_pduty, "<ch> <percent>",       "set duty cycle (0..100, 0.1 steps)" },
    { "stop", sub_pstop, "<ch>",                 "stop and drive the pin low" },
    { "fade", sub_pfade, "<ch> [period_ms]",     "LED breathing effect" },
    { NULL, NULL, NULL, NULL }
};

/* --------------------------- servo -------------------------------- */

static int cmd_servo(int argc, char *argv[])
{
    hal_pwm_t ch;
    int32_t   angle;
    if ((argc != 3) || !parse_pwm(argv[1], &ch) || !cli_arg_i32(argv[2], &angle)) {
        return CLI_ERR_USAGE;
    }
    if ((angle < 0) || (angle > 180)) {
        return cli_error("angle 0..180");
    }
    if (hal_pwm_get_frequency(ch) != 50U) {
        (void)hal_pwm_init(ch, 50U);           /* hobby servos: 50 Hz frame */
    }
    /* 1.0 ms .. 2.0 ms pulse for 0 .. 180 degrees */
    const uint32_t us = 1000U + (((uint32_t)angle * 1000U) / 180U);
    hal_pwm_set_pulse_us(ch, us);
    io_printf("servo %ld deg -> %lu us pulse" IO_NEWLINE, (long)angle, (unsigned long)us);
    return CLI_OK;
}

/* ---------------------------- tone -------------------------------- */

static int cmd_tone(int argc, char *argv[])
{
    hal_pwm_t ch;
    uint32_t  hz;
    uint32_t  ms = 300U;
    if ((argc < 3) || !parse_pwm(argv[1], &ch) || !cli_arg_u32_range(argv[2], 20U, 20000U, &hz)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 4) && !cli_arg_u32_range(argv[3], 1U, 10000U, &ms)) {
        return CLI_ERR;
    }
    if (hal_pwm_init(ch, hz) != HAL_OK) {
        return cli_error("cannot reach %lu Hz", (unsigned long)hz);
    }
    hal_pwm_set_duty(ch, 500U);                /* square wave for a buzzer */
    hal_delay_ms(ms);
    hal_pwm_set_duty(ch, 0U);
    return CLI_OK;
}

static const cli_command_t timer_cmds[] = {
    { "timer", NULL,      "<sub> ...",             "general purpose timers TIM2..4", timer_subs },
    { "pwm",   NULL,      "<sub> ...",             "PWM outputs on TIM3/TIM4",        pwm_subs },
    { "servo", cmd_servo, "<ch> <0..180>",         "hobby servo angle (50 Hz)",       NULL },
    { "tone",  cmd_tone,  "<ch> <hz> [ms]",        "beep on a passive buzzer",        NULL },
};

static const cli_group_t timer_group = { "timers", timer_cmds, sizeof(timer_cmds) / sizeof(timer_cmds[0]) };

const cli_group_t *cmd_timer_group(void)
{
    return &timer_group;
}

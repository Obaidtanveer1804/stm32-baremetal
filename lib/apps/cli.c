/**
 * @file    cli.c
 * @brief   The full interactive shell with every command group, plus
 *          a cooperative scheduler running background tasks (heartbeat
 *          LED, button events, watchdog kick) while you type.
 *
 *   PC   : make run APP=cli          (your terminal is the UART)
 *   board: make TARGET=f100 APP=cli flash ; picocom -b 115200 /dev/ttyUSB0
 */
#include "hal.h"
#include "io.h"
#include "log.h"
#include "sched.h"
#include "cli.h"
#include "cli_cmds.h"
#include "led.h"
#include "button.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define TASK_CLI_MS        (5U)
#define TASK_LED_MS        (10U)
#define TASK_BUTTON_MS     (5U)

static led_t    s_heartbeat;
static button_t s_button;

static void task_cli(void *arg)
{
    (void)arg;
    cli_poll();
}

static void task_led(void *arg)
{
    (void)arg;
    led_update(&s_heartbeat, hal_time_ms());
}

static void task_button(void *arg)
{
    (void)arg;
    const btn_event_t ev = button_update(&s_button, hal_time_ms());
    if ((ev == BTN_EVENT_CLICK) || (ev == BTN_EVENT_DOUBLE_CLICK) || (ev == BTN_EVENT_LONG_PRESS)) {
        LOG_I("btn", "%s", btn_event_name(ev));
    }
}

static void task_watchdog(void *arg)
{
    (void)arg;
    hal_wdg_kick();
}

static void print_banner(void)
{
    io_ansi_color(IO_COLOR_CYAN);
    io_println("");
    io_println("  ___ _    ___ ___ ___   ___ _    ___ ");
    io_println(" | __| |  |_ _/ __| __| / __| |  |_ _|");
    io_println(" | _|| |__ | |\\__ \\ _| | (__| |__ | | ");
    io_println(" |___|____|___|___/___| \\___|____|___|");
    io_ansi_color(IO_COLOR_RESET);
    io_printf("  %s, %s @ %lu MHz" IO_NEWLINE, BOARD_NAME, hal_sys_name(),
              (unsigned long)(hal_sys_clock_hz() / 1000000U));
    io_println("  type 'help', TAB completes, arrows recall history");
    io_println("");
}

int main(void)
{
    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    log_init(hal_time_ms);
#ifdef BOARD_PC
    sim_devices_install_default();
#endif

    led_init(&s_heartbeat, BOARD_LED_BLUE, BOARD_LED_ACTIVE_HIGH != 0);
    led_heartbeat(&s_heartbeat);
    button_init(&s_button, BOARD_BUTTON, BOARD_BUTTON_ACTIVE_HIGH != 0, NULL);

    print_banner();
    if ((hal_sys_reset_cause() & HAL_RESET_IWDG) != 0U) {
        LOG_W("sys", "last reset was caused by the watchdog");
    }

    cli_init("elise> ");
    cli_register_all();

    sched_init(hal_time_ms, hal_time_us);
    (void)sched_add("cli",    task_cli,      NULL, TASK_CLI_MS);
    (void)sched_add("led",    task_led,      NULL, TASK_LED_MS);
    (void)sched_add("button", task_button,   NULL, TASK_BUTTON_MS);
    (void)sched_add("wdg",    task_watchdog, NULL, 100U);

    cli_print_prompt();
    sched_run_forever(hal_sys_idle);
    return 0;
}

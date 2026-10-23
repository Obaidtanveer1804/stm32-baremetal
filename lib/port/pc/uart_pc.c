/**
 * @file    uart_pc.c
 * @brief   Simulated UARTs. The console UART is the terminal: stdin is
 *          polled from the 1 ms tick (= RX interrupt) into the same
 *          ring buffer the F1 port uses; TX goes straight to stdout.
 *          Other UARTs log their traffic and can loop back.
 *
 * When stdin is a pipe or a file (./app.pc < script.txt), reaching the
 * end of the input and reading an empty buffer ends the simulation:
 * handy for automated tests of the CLI.
 */
#define _POSIX_C_SOURCE 200809L

#include "sim_internal.h"
#include "ringbuf.h"
#include "board.h"

#include <errno.h>
#include <unistd.h>

#define STDIN_CHUNK  (64U)

static uint8_t           s_rx_storage[HAL_UART_COUNT][HAL_UART_RX_BUFFER_SIZE];
static ringbuf_t         s_rx[HAL_UART_COUNT];
static volatile uint32_t s_rx_dropped[HAL_UART_COUNT];
static bool              s_ready[HAL_UART_COUNT];
static bool              s_loopback[HAL_UART_COUNT];
static uint32_t          s_baud[HAL_UART_COUNT];
static volatile bool     s_stdin_eof;
static bool              s_console_read;   /* the app reads the console */

static void ensure_buffers(hal_uart_t uart)
{
    if (!s_ready[uart]) {
        (void)ringbuf_init(&s_rx[uart], s_rx_storage[uart], HAL_UART_RX_BUFFER_SIZE);
        s_ready[uart] = true;
    }
}

hal_status_t hal_uart_init(hal_uart_t uart, uint32_t baud)
{
    if ((uart >= HAL_UART_COUNT) || (baud == 0U)) {
        return HAL_EINVAL;
    }
    sim_core_ensure_started();
    ensure_buffers(uart);
    s_baud[uart] = baud;
    sim_log(SIM_LOG_UART, "uart%u init %lu baud%s", (unsigned)uart + 1U,
            (unsigned long)baud, (uart == BOARD_CONSOLE_UART) ? " (console = terminal)" : "");
    return HAL_OK;
}

void hal_uart_putc(hal_uart_t uart, char ch)
{
    if (uart >= HAL_UART_COUNT) {
        return;
    }
    if (uart == BOARD_CONSOLE_UART) {
        sim_raw_write(&ch, 1U);
        return;
    }
    sim_log(SIM_LOG_UART, "uart%u tx 0x%02X '%c'", (unsigned)uart + 1U, (unsigned)(uint8_t)ch,
            ((ch >= 0x20) && (ch < 0x7F)) ? ch : '.');
    if (s_loopback[uart]) {
        ensure_buffers(uart);
        if (!ringbuf_put(&s_rx[uart], (uint8_t)ch)) {
            s_rx_dropped[uart]++;
        }
    }
}

void hal_uart_write(hal_uart_t uart, const void *data, size_t len)
{
    const char *p = (const char *)data;
    if (uart == BOARD_CONSOLE_UART) {
        sim_raw_write(p, len);      /* one syscall instead of len */
        return;
    }
    while (len-- > 0U) {
        hal_uart_putc(uart, *p++);
    }
}

void hal_uart_puts(hal_uart_t uart, const char *s)
{
    size_t n = 0U;
    while (s[n] != '\0') {
        n++;
    }
    hal_uart_write(uart, s, n);
}

int hal_uart_getc(hal_uart_t uart)
{
    uint8_t b;

    if (uart >= HAL_UART_COUNT) {
        return -1;
    }
    sim_core_ensure_started();
    ensure_buffers(uart);
    if (uart == BOARD_CONSOLE_UART) {
        s_console_read = true;
    }
    if (ringbuf_get(&s_rx[uart], &b)) {
        return (int)b;
    }
    if ((uart == BOARD_CONSOLE_UART) && s_stdin_eof) {
        return -2;             /* script finished: hal_sys_idle powers off */
    }
    return -1;
}

uint32_t hal_uart_rx_available(hal_uart_t uart)
{
    return ((uart < HAL_UART_COUNT) && s_ready[uart]) ? ringbuf_count(&s_rx[uart]) : 0U;
}

uint32_t hal_uart_rx_dropped(hal_uart_t uart)
{
    return (uart < HAL_UART_COUNT) ? s_rx_dropped[uart] : 0U;
}

void hal_uart_rx_flush(hal_uart_t uart)
{
    if ((uart < HAL_UART_COUNT) && s_ready[uart]) {
        ringbuf_reset(&s_rx[uart]);
    }
}

void hal_uart_tx_wait(hal_uart_t uart)
{
    (void)uart;   /* writes are synchronous on the PC */
}

/* ------------------------------ sim API --------------------------- */

void sim_uart_loopback(hal_uart_t uart, bool enable)
{
    if (uart < HAL_UART_COUNT) {
        s_loopback[uart] = enable;
    }
}

void sim_uart_inject(hal_uart_t uart, const uint8_t *data, size_t len)
{
    if (uart >= HAL_UART_COUNT) {
        return;
    }
    ensure_buffers(uart);
    const uint32_t irq = hal_irq_save();
    for (size_t i = 0U; i < len; i++) {
        if (!ringbuf_put(&s_rx[uart], data[i])) {
            s_rx_dropped[uart]++;
        }
    }
    hal_irq_restore(irq);
}

/* Called from the tick: the "RX interrupt" of the console UART. */
void sim_uart_tick(void)
{
    uint8_t buf[STDIN_CHUNK];

    if (s_stdin_eof) {
        return;
    }
    ensure_buffers(BOARD_CONSOLE_UART);
    /* never read more than fits: unread bytes stay in the pipe
     * (flow control for free, no loss with long scripts) */
    uint32_t room = ringbuf_free(&s_rx[BOARD_CONSOLE_UART]);
    if (room > STDIN_CHUNK) {
        room = STDIN_CHUNK;
    }
    if (room == 0U) {
        return;
    }
    const ssize_t n = read(STDIN_FILENO, buf, room);
    if (n > 0) {
        (void)ringbuf_write(&s_rx[BOARD_CONSOLE_UART], buf, (uint16_t)n);
    } else if ((n == 0) && !sim_stdin_is_tty()) {
        s_stdin_eof = true;
    } else {
        /* EAGAIN: nothing typed */
    }
}

bool sim_stdin_eof(void)
{
    /* only meaningful for apps that read the console (the CLI): an app
     * that never reads stdin keeps running when started < /dev/null */
    return s_stdin_eof && s_console_read;
}

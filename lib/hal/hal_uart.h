/**
 * @file    hal_uart.h
 * @brief   UART with interrupt-driven RX ring buffer, blocking TX.
 *
 * Pins (STM32F1 default mapping):
 *   UART1: TX PA9,  RX PA10   (VL-Discovery: free, use a USB-serial)
 *   UART2: TX PA2,  RX PA3
 *   UART3: TX PB10, RX PB11
 *
 * The console (CLI, io.h) uses HAL_CONSOLE_UART (board.h).
 * On the PC port the console is the terminal (stdin/stdout), other
 * UARTs print their traffic and loop back if sim loopback is on.
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include "hal_types.h"

typedef enum {
    HAL_UART1 = 0,
    HAL_UART2,
    HAL_UART3,
    HAL_UART_COUNT
} hal_uart_t;

#define HAL_UART_RX_BUFFER_SIZE  (128U)   /* power of two */

hal_status_t hal_uart_init(hal_uart_t uart, uint32_t baud);
void         hal_uart_putc(hal_uart_t uart, char ch);
void         hal_uart_write(hal_uart_t uart, const void *data, size_t len);
void         hal_uart_puts(hal_uart_t uart, const char *s);
/** Next received byte, -1 if none (non-blocking), or -2 when the input
 *  is closed for good (PC port: end of a piped script). */
int          hal_uart_getc(hal_uart_t uart);
/** Bytes waiting in the RX buffer. */
uint32_t     hal_uart_rx_available(hal_uart_t uart);
/** Bytes lost because the RX buffer was full (overrun counter). */
uint32_t     hal_uart_rx_dropped(hal_uart_t uart);
void         hal_uart_rx_flush(hal_uart_t uart);
/** Wait until the last byte left the shift register. */
void         hal_uart_tx_wait(hal_uart_t uart);

/* Console shortcuts with the io.h signatures: io_init(hal_console_putc,
 * hal_console_getc) wires printf-like output to the console UART. */
void hal_console_init(uint32_t baud);
void hal_console_putc(char ch);
int  hal_console_getc(void);

#endif /* HAL_UART_H */

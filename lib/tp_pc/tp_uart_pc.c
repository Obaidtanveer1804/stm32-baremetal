/**
 * @file    tp_uart_pc.c
 * @brief   PC version of the TP's uart_f1.c (the uart.h API) built on the
 *          ELISE library: my_puts/my_putchar go to the simulated USART1
 *          console (your terminal), the LEDs are the simulated PC1/PC2 of
 *          the F103 board, delay() takes about as long as on the chip.
 *
 * The prototypes come from the TP's own uart.h, so the compiler checks
 * that this file and uart_f1.c offer exactly the same functions.
 */
#include "uart.h"

#include "hal.h"
#include "fmt.h"
#include "sim.h"

/* F103 board pins, as in uart_f1.c (non-VL branch) */
#define TP_LED_PORT          (HAL_PORT_C)
#define TP_LED_MASK          ((uint16_t)((1U << 1) | (1U << 2)))   /* PC1 | PC2 */

/*
 * delay(n) is a NOP loop on the chip: at 72 MHz and -O0 one turn of the
 * volatile loop takes about 12 cycles, i.e. ~6000 turns per ms. Waiting
 * the same real time keeps the TP's blink and print rhythm.
 */
#define TP_DELAY_LOOPS_PER_MS (6000U)

static void putchar_sink(char ch, void *ctx)
{
    (void)ctx;
    my_putchar((unsigned char)ch);
}

void core_clock_setup(void)
{
    hal_sys_init();
}

void clock_setup(void)
{
    core_clock_setup();
    hal_time_init();
#ifndef TP_PC_QUIET
    sim_set_log(SIM_LOG_GPIO);        /* show every LED change */
#endif
}

void init_gpio(void)
{
    hal_gpio_port_mode(TP_LED_PORT, TP_LED_MASK, HAL_GPIO_OUTPUT);
}

void usart_setup(void)
{
    hal_console_init(BOARD_CONSOLE_BAUD);
}

void my_putchar(unsigned char ch)
{
    hal_console_putc((char)ch);
}

/* Same subset as uart_f1.c (%d %u %x %c %s %%) and more, same output. */
void my_puts(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    (void)fmt_vformat(putchar_sink, NULL, fmt, args);
    va_end(args);
}

void delay(unsigned int delay)
{
    hal_delay_ms(delay / TP_DELAY_LOOPS_PER_MS);
}

void led_set(int msk)
{
    (void)msk;                        /* the F103 branch ignores it too */
    hal_gpio_port_write(TP_LED_PORT, TP_LED_MASK, 0U);
}

void led_clr(int msk)
{
    (void)msk;
    hal_gpio_port_write(TP_LED_PORT, 0U, TP_LED_MASK);
}

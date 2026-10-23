/**
 * @file    main_hello.c
 * @brief   TP-style program built on the ELISE library:
 *          make PROG=hello USE_LIB=1 flash          (F103 board)
 *          make PROG=hello USE_LIB=1 BOARD=f100 flash
 *
 * The same code runs on both boards: BOARD_LED_BLUE is PC1 on the F103
 * board and PC8 on the VL-Discovery (lib/hal/board.h).
 */
#include "hal.h"
#include "io.h"
#include "counter.h"

#define HELLO_PERIOD_MS  (500U)
#define HELLO_COUNT_MAX  (255)
#define HELLO_BITS       (8U)

int main(void)
{
    counter_t count;

    hal_sys_init();                          /* clock: 72 MHz F103, 24 MHz F100 */
    hal_time_init();                         /* 1 ms SysTick */
    hal_console_init(BOARD_CONSOLE_BAUD);    /* USART1 PA9/PA10, 115200 */
    io_init(hal_console_putc, hal_console_getc);

    hal_gpio_mode(BOARD_LED_BLUE, HAL_GPIO_OUTPUT);
    hal_gpio_mode(BOARD_LED_GREEN, HAL_GPIO_OUTPUT);
    hal_gpio_set(BOARD_LED_BLUE);            /* LEDs in opposite phases */
    counter_init(&count, 0, HELLO_COUNT_MAX, 1, COUNTER_MODE_WRAP);

    io_printf("hello from %s" IO_NEWLINE, BOARD_NAME);
    for (;;) {
        counter_print_all((uint32_t)counter_get(&count), HELLO_BITS);
        hal_gpio_toggle(BOARD_LED_BLUE);
        hal_gpio_toggle(BOARD_LED_GREEN);
        (void)counter_step(&count);
        hal_delay_ms(HELLO_PERIOD_MS);
    }
    return 0;
}

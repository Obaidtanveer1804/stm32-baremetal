/**
 * @file    blink.c
 * @brief   Smallest possible app: toggle both LEDs every 500 ms.
 *          Good first flash test of a new board.
 */
#include "hal.h"

#define BLINK_HALF_PERIOD_MS  (500U)

int main(void)
{
    hal_sys_init();
    hal_time_init();
    hal_gpio_mode(BOARD_LED_BLUE, HAL_GPIO_OUTPUT);
    hal_gpio_mode(BOARD_LED_GREEN, HAL_GPIO_OUTPUT);
    hal_gpio_set(BOARD_LED_BLUE);           /* start in opposite phases */
    for (;;) {
        hal_gpio_toggle(BOARD_LED_BLUE);
        hal_gpio_toggle(BOARD_LED_GREEN);
        hal_delay_ms(BLINK_HALF_PERIOD_MS);
    }
    return 0;
}

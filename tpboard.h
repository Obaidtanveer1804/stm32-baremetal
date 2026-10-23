/* Board selection for the TP files (uart_f1.c, cgpio.c).
 * Chosen by the Makefile: make BOARD=f103 | f100 | pc  (-DBOARD_xxx, same
 * names as lib/hal/board.h). On the PC, <libopencm3/stm32/gpio.h> is the
 * stand-in of lib/tp_pc, so gpio_set(GPIOC, ...) prints "[sim] PC1 -> 1". */
#ifndef TPBOARD_H
#define TPBOARD_H

#if defined(BOARD_VLDISCOVERY)        /* STM32F100RB: 24 MHz, 128K flash, 8K RAM */
 #define TP_BOARD_NAME   "STM32VL-Discovery (F100)"
 #define TP_CLOCK_24MHZ
 #define TP_LED_INIT     (GPIO7|GPIO8|GPIO9)  /* PC7 PC8 (blue) PC9 (green) */
 #define TP_LED_ALL      (GPIO8|GPIO9)
 #define TP_LED_SELECT                        /* led_set(0/1/3) picks one LED */
 #define TP_LED_0        GPIO8
 #define TP_LED_1        GPIO9
 #define TP_LED_3        GPIO7
#elif defined(BOARD_F103_CUSTOM) || defined(BOARD_PC)  /* STM32F103: 72 MHz, LEDs PC1/PC2 */
 #define TP_BOARD_NAME   "STM32F103 custom"
 #define TP_LED_INIT     (GPIO1|GPIO2)
 #define TP_LED_ALL      (GPIO1|GPIO2)
#else
 #error "Define a board: -DBOARD_F103_CUSTOM, -DBOARD_VLDISCOVERY or -DBOARD_PC (make BOARD=f103|f100|pc)"
#endif

#endif

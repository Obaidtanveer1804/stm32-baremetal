/**
 * @file    board.h
 * @brief   Board description: which pins the LEDs / button / console
 *          use. Selected with -DBOARD_xxx in the Makefile.
 *
 * Application code says BOARD_LED_GREEN, never PC9, so the same main
 * runs on the VL-Discovery, the custom F103 board and the PC.
 */
#ifndef BOARD_H
#define BOARD_H

#include "hal_gpio.h"
#include "hal_uart.h"

#if defined(BOARD_VLDISCOVERY) || defined(BOARD_PC)
/* STM32VL-Discovery (STM32F100RB, 24 MHz). The PC simulation mirrors
 * it so the same app can be run on both. */
#define BOARD_NAME              "STM32VL-Discovery"
#define BOARD_LED_BLUE          PC(8)
#define BOARD_LED_GREEN         PC(9)
#define BOARD_LED_COUNT         (2U)
#define BOARD_LEDS_INIT         { BOARD_LED_BLUE, BOARD_LED_GREEN }
#define BOARD_LED_ACTIVE_HIGH   (1)
#define BOARD_BUTTON            PA(0)      /* USER button, active high, ext. pull-down */
#define BOARD_BUTTON_ACTIVE_HIGH (1)
#define BOARD_CONSOLE_UART      HAL_UART1  /* PA9 TX / PA10 RX */
#define BOARD_TIM3_FULL_REMAP   (1)        /* TIM3 CH3/CH4 drive the LEDs */
#define BOARD_HAS_DAC           (1)
#define BOARD_HAS_LSE           (0)
#define BOARD_PWM_LED_BLUE      HAL_PWM_T3C3
#define BOARD_PWM_LED_GREEN     HAL_PWM_T3C4

#elif defined(BOARD_F103_CUSTOM)
/* Custom STM32F103 board of the TP (LEDs on PC1/PC2, FTDI flashing). */
#define BOARD_NAME              "STM32F103 custom"
#define BOARD_LED_BLUE          PC(1)
#define BOARD_LED_GREEN         PC(2)
#define BOARD_LED_COUNT         (2U)
#define BOARD_LEDS_INIT         { BOARD_LED_BLUE, BOARD_LED_GREEN }
#define BOARD_LED_ACTIVE_HIGH   (1)
#define BOARD_BUTTON            PA(0)
#define BOARD_BUTTON_ACTIVE_HIGH (0)       /* to GND with internal pull-up */
#define BOARD_CONSOLE_UART      HAL_UART1
#define BOARD_TIM3_FULL_REMAP   (0)
#define BOARD_HAS_DAC           (0)
#define BOARD_HAS_LSE           (0)
#define BOARD_PWM_LED_BLUE      HAL_PWM_T3C1   /* LEDs are not on PWM pins */
#define BOARD_PWM_LED_GREEN     HAL_PWM_T3C2

#elif defined(BOARD_BLUEPILL)
/* "Blue pill" STM32F103C8T6, LED on PC13 (active low). */
#define BOARD_NAME              "Blue Pill F103C8"
#define BOARD_LED_BLUE          PC(13)
#define BOARD_LED_GREEN         PC(13)
#define BOARD_LED_COUNT         (1U)
#define BOARD_LEDS_INIT         { BOARD_LED_GREEN }
#define BOARD_LED_ACTIVE_HIGH   (0)
#define BOARD_BUTTON            PA(0)
#define BOARD_BUTTON_ACTIVE_HIGH (0)
#define BOARD_CONSOLE_UART      HAL_UART1
#define BOARD_TIM3_FULL_REMAP   (0)
#define BOARD_HAS_DAC           (0)
#define BOARD_HAS_LSE           (1)
#define BOARD_PWM_LED_BLUE      HAL_PWM_T3C1
#define BOARD_PWM_LED_GREEN     HAL_PWM_T3C2

#else
#error "Define a board: -DBOARD_VLDISCOVERY, -DBOARD_F103_CUSTOM, -DBOARD_BLUEPILL or -DBOARD_PC"
#endif

#define BOARD_CONSOLE_BAUD      (115200U)

#endif /* BOARD_H */

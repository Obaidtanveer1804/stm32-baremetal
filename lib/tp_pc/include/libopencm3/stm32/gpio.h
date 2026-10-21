/**
 * @file    gpio.h  (PC stand-in for <libopencm3/stm32/gpio.h>, STM32F1)
 * @brief   The libopencm3 GPIO API your TP code calls, with the same
 *          names, values and port addresses, implemented on top of the
 *          ELISE simulator (lib/port/pc). Lets cgpio.c, main_led.c...
 *          compile and run on the PC without changing a line.
 *
 * Only this include directory differs between the board build and the PC
 * build: on the board the real libopencm3 header is found instead.
 */
#ifndef TP_PC_LIBOPENCM3_GPIO_H
#define TP_PC_LIBOPENCM3_GPIO_H

#include <stdint.h>

/* Port base addresses: same values as libopencm3 (APB2 + 0x0800...). */
#define GPIO_PORT_A_BASE   (0x40010800U)
#define GPIO_PORT_B_BASE   (0x40010C00U)
#define GPIO_PORT_C_BASE   (0x40011000U)
#define GPIO_PORT_D_BASE   (0x40011400U)
#define GPIO_PORT_E_BASE   (0x40011800U)
#define GPIOA              GPIO_PORT_A_BASE
#define GPIOB              GPIO_PORT_B_BASE
#define GPIOC              GPIO_PORT_C_BASE
#define GPIOD              GPIO_PORT_D_BASE
#define GPIOE              GPIO_PORT_E_BASE

/* Pin masks */
#define GPIO0    (1U << 0)
#define GPIO1    (1U << 1)
#define GPIO2    (1U << 2)
#define GPIO3    (1U << 3)
#define GPIO4    (1U << 4)
#define GPIO5    (1U << 5)
#define GPIO6    (1U << 6)
#define GPIO7    (1U << 7)
#define GPIO8    (1U << 8)
#define GPIO9    (1U << 9)
#define GPIO10   (1U << 10)
#define GPIO11   (1U << 11)
#define GPIO12   (1U << 12)
#define GPIO13   (1U << 13)
#define GPIO14   (1U << 14)
#define GPIO15   (1U << 15)
#define GPIO_ALL (0xFFFFU)

/* gpio_set_mode() arguments (CRL/CRH MODE and CNF fields) */
#define GPIO_MODE_INPUT                 (0x00U)
#define GPIO_MODE_OUTPUT_10_MHZ         (0x01U)
#define GPIO_MODE_OUTPUT_2_MHZ          (0x02U)
#define GPIO_MODE_OUTPUT_50_MHZ         (0x03U)

#define GPIO_CNF_INPUT_ANALOG           (0x00U)
#define GPIO_CNF_INPUT_FLOAT            (0x01U)
#define GPIO_CNF_INPUT_PULL_UPDOWN      (0x02U)
#define GPIO_CNF_OUTPUT_PUSHPULL        (0x00U)
#define GPIO_CNF_OUTPUT_OPENDRAIN       (0x01U)
#define GPIO_CNF_OUTPUT_ALTFN_PUSHPULL  (0x02U)
#define GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN (0x03U)

/* A few alternate-function pin names the TPs use */
#define GPIO_USART1_TX   GPIO9      /* PA9  */
#define GPIO_USART1_RX   GPIO10     /* PA10 */
#define GPIO_USART2_TX   GPIO2      /* PA2  */
#define GPIO_USART2_RX   GPIO3      /* PA3  */
#define GPIO_SPI1_SCK    GPIO5      /* PA5  */
#define GPIO_SPI1_MISO   GPIO6      /* PA6  */
#define GPIO_SPI1_MOSI   GPIO7      /* PA7  */
#define GPIO_SPI1_NSS    GPIO4      /* PA4  */

void     gpio_set_mode(uint32_t gpioport, uint8_t mode, uint8_t cnf, uint16_t gpios);
void     gpio_set(uint32_t gpioport, uint16_t gpios);
void     gpio_clear(uint32_t gpioport, uint16_t gpios);
void     gpio_toggle(uint32_t gpioport, uint16_t gpios);
uint16_t gpio_get(uint32_t gpioport, uint16_t gpios);
uint16_t gpio_port_read(uint32_t gpioport);
void     gpio_port_write(uint32_t gpioport, uint16_t data);

#endif /* TP_PC_LIBOPENCM3_GPIO_H */

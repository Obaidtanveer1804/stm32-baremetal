#include <stdarg.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/flash.h> // definitions du timer
#include "tpboard.h"

#define usart1

void core_clock_setup(void)
{
#ifdef QEMU
 // qemu does not model the PLL/RCC: waiting for PLLRDY would hang, keep the reset clock
#elif defined(TP_CLOCK_24MHZ)
 rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_24MHZ]);  // STM32F100 discovery
#else
 rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);  // STM32F103
#endif
}

void my_putchar(unsigned char ch)
{
	usart_send_blocking(USART1, ch);
//	usart_send_blocking(USART2, ch);
}

static void put_number(unsigned int n, unsigned int base)
{
	char buf[11];
	int i = 0;
	do {
		buf[i++] = "0123456789ABCDEF"[n % base];
		n /= base;
	} while (n);
	while (i--)
		my_putchar(buf[i]);
}

void my_puts(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			my_putchar(*fmt);
			continue;
		}
		switch (*++fmt) {
		case 'd': {
			int n = va_arg(args, int);
			if (n < 0) {
				my_putchar('-');
				n = -n;
			}
			put_number(n, 10);
			break;
		}
		case 'u': put_number(va_arg(args, unsigned int), 10); break;
		case 'x': put_number(va_arg(args, unsigned int), 16); break;
		case 'c': my_putchar(va_arg(args, int)); break;
		case 's': my_puts(va_arg(args, char *)); break;
		case '%': my_putchar('%'); break;
		default:  my_putchar('%'); my_putchar(*fmt); break;
		}
	}
	va_end(args);
}

void delay(unsigned int delay)
{
	volatile unsigned int i;
	for(i=0;i<delay;i++)
		__asm__("nop");
}


void init_gpio(void)
{
	/* Setup GPIO for LED. */
 gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL,TP_LED_INIT);

#ifdef usart1
  gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);
#else
  gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART2_TX);
#endif
}

void clock_setup(void)
{
// for STM32F4-Disco
// 	rcc_clock_setup_hse_3v3(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

// for DDS-based project
	core_clock_setup();
	/* Enable GPIOD clock for LED & USARTs. */
#ifdef netwk
	rcc_periph_clock_enable(RCC_GPIOA);
#else
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOC);
#endif

	/* Enable clocks for USART2. */
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_USART2);
}


void led_set(int msk)
{
#ifdef TP_LED_SELECT
	switch (msk)
         {case 0: gpio_set(GPIOC, TP_LED_0);break; // blue
          case 1: gpio_set(GPIOC, TP_LED_1); break; // green
          case 3: gpio_set(GPIOC, TP_LED_3); break;
          default:gpio_set(GPIOC, TP_LED_ALL);
         }
#else
#ifdef netwk
	gpio_set(GPIOA, msk);
#else
	gpio_set(GPIOC, TP_LED_ALL);
#endif
#endif
}

void led_clr(int msk)
{
#ifdef TP_LED_SELECT
	switch (msk)
         {case 0: gpio_clear(GPIOC, TP_LED_0);break; // blue
          case 1: gpio_clear(GPIOC, TP_LED_1); break; // green
          case 3: gpio_clear(GPIOC, TP_LED_3); break;
          default:gpio_clear(GPIOC, TP_LED_ALL);
         }
#else
#ifdef netwk
	gpio_clear(GPIOA, msk);
#else
	gpio_clear(GPIOC, TP_LED_ALL);
#endif
#endif
}

void usart_setup(void)
{
	/* Setup USART1 parameters. */
	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART1);
	
        /* Setup USART2 parameters. */
	usart_set_baudrate(USART2, 115200);
	usart_set_databits(USART2, 8);
	usart_set_stopbits(USART2, USART_STOPBITS_1);
	usart_set_mode(USART2, USART_MODE_TX_RX);
	usart_set_parity(USART2, USART_PARITY_NONE);
	usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART2);
}


#include <libopencm3/stm32/gpio.h>
#include "tpboard.h"

void blink_led(void){
	 gpio_toggle(GPIOC, TP_LED_ALL);

}

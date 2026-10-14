
#include <libopencm3/stm32/gpio.h>

void blink_led(void){
	 gpio_toggle(GPIOC, GPIO1|GPIO2);

}

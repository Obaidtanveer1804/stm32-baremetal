#include <libopencm3/stm32/gpio.h>
#include "uart.h"

int main(){
	clock_setup();
	init_gpio();

	while(1){
		gpio_toggle(GPIOC, GPIO1); // LEDs on PC1/PC2 (STM32F103 board)
		delay(800000);
	}
	return 0;
}

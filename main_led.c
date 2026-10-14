#include <libopencm3/stm32/gpio.h>
#include "uart.h"
#include "tp2.h"
#include "cgpio.h"

int main(){
	clock_setup();
	init_gpio();
	usart_setup();

	int icounter = 0;
	
	while(1){
		blink_led();
		icounter = counter(icounter);
		tp2_task5_count_hex(icounter);
	        tp2_task6_count_dec(icounter);
		delay(800000);
	}
		
	return 0;
}

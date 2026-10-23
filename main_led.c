#include <libopencm3/stm32/gpio.h>
#include "uart.h"
#include "tp3.h"
#include "cgpio.h"
#include "newtonsq.h"
#include "fact.h"

int main(){
	clock_setup();
	init_gpio();
	usart_setup();

	int icounter = 0;
	uint32_t num = 4;
	uint32_t sqrt = 0;
	int val = 5;
	while(1){
		blink_led();
//		sqrt = newtonsq(num);
//		my_puts("number = %d ",num);
//		my_puts("sqrt %d \r\n",sqrt);
		my_puts("fact(%d) = ",val);
		my_puts("%d \r\n",fact(val));
		num += 5;
//		TP3 here;
//		ad_freq(4000000);
//		TP2 here;
//		icounter = counter(icounter); tp2_task5_count_hex(icounter); 
//		tp2_task6_count_dec(icounter);
		delay(80000);
	}
		
	return 0;
}

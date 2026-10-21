#include <libopencm3/stm32/gpio.h>
#include "uart.h"
#include "tp3.h"
#include "cgpio.h"
#include "newtonsq.h"

int main(){
	clock_setup();
	init_gpio();
	usart_setup();

	int icounter = 0;
	uint32_t num = 4;
	uint32_t sqrt = 0;	

	while(1){
		blink_led();
//------------------------------------ newton square root fucntion using integers ---------------------------------------------------

//		sqrt = newtonsq(num);
//		my_puts("number = %u ",num);
//		my_puts("sqrt %u \r\n",sqrt);
//		num += 5;

//-------------------------------------------------- TP3 here;---------------------------------------------------------

//		ad_freq(4000000);

//---------------------------------------------------TP2 here;----------------------------------------------------------------------------------------------------

//		icounter = counter(icounter); tp2_task5_count_hex(icounter); 
//		tp2_task6_count_dec(icounter);

//-----------------------------------------------------------------------------------------------------------------------

		delay(800000);
	}
		
	return 0;
}

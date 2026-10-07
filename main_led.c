#include "uart.h"

int main(){
	clock_setup();
	init_gpio();
	led_set(1);
	led_clr(0);

	while(1){
		led_set(0);led_clr(1);delay(800000);
		led_set(1);led_clr(0);delay(800000);
}
return 0;
}

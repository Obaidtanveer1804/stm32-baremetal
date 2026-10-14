#include <libopencm3/stm32/gpio.h>
#include "uart.h"
#include "tp2.h"

int main(){
	clock_setup();
	init_gpio();
	usart_setup();

	tp2_task5_count_hex();
	tp2_task6_count_dec();

	int icounter = 0;
	int ibinary;
	int binaryNum[8];
	int arraycount;

	while(1){
		gpio_toggle(GPIOC, GPIO1|GPIO2);
		delay(800000);
		my_puts("hello from 103 \r\n");
		my_puts("decimal counter : %d\r\n", icounter);

		ibinary = icounter;
		arraycount = 0;
		while (ibinary > 0){
			binaryNum[arraycount] = ibinary%2;
			ibinary = ibinary/2;
			arraycount++;
		}

		my_puts("binary counter : ");
		if (arraycount == 0){
			my_puts("0");
		}
		while (arraycount > 0){
			arraycount--;
			my_puts("%d", binaryNum[arraycount]);
		}
		my_puts("\r\n");

		if (icounter == 255){
			icounter = 0;
		}else{
			icounter++;
		}
	}
	return 0;
}

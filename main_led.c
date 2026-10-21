#include <libopencm3/stm32/gpio.h>
#include "uart.h"
#include "tp2.h"
#include "cgpio.h"
#include "gettime.h"
#include <stdio.h>

int main(){
	clock_setup();
	init_gpio();
	usart_setup();
	gettime_init();
	int icounter = 0;
	uint64_t t1 =0;
	uint64_t t2 = 0;

	while(1){
		blink_led();
		icounter = counter(icounter);
		tp2_task5_count_hex(icounter);
	        tp2_task6_count_dec(icounter);

		t1= gettime();
		printf("printf to check for time w print f \r\n");
		t2 = gettime();
		uint64_t cycles = t2 - t1;
		printf("time for print f : %lu us \r\n ", gettime_cycles_to_us(cycles, gettime_cpu_freq()));
		
		t1= gettime();
                my_puts("printf to check for time w my Puts (custom) \r\n");
                t2 = gettime();
                cycles = t2 - t1;
                printf("time for my puts : %lu us \r\n ", gettime_cycles_to_us(cycles, gettime_cpu_freq()));

		printf("cpu freq:%lu hz \r\n",gettime_cpu_freq());
		delay(800000);
	}
		
	return 0;
}

#include <libopencm3/stm32/gpio.h>
#include "uart.h"
#include "cgpio.h"
#include "fixpoint.h"
#include "lfsr.h"

void print_fx(long a){
	long ip = a/DECIMALES;                  // before the dot: 1048808 -> 1
	long f  = a-ip*DECIMALES;               // after the dot:  1048808 -> 48808 (the 0 is lost)
	long d;
	my_puts("%d.",(int)ip);
	for (d=DECIMALES/10; d>1 && f<d; d/=10) my_puts("0");   // one "0" per missing digit
	my_puts("%d",(int)f);
}

void print_sqrt(char *name, long long num, long long den){
	long res = jmsqrt(num*DECIMALES*DECIMALES/den);   // sqrt(x)*DECIMALES, radicand = x*DECIMALES^2
	my_puts("%s = ", name);
	print_fx(res);
	my_puts("\r\n");
}

void demo_fpa(void){
	print_sqrt("sqrt(2)        ", 2, 1);
	print_sqrt("sqrt(142)      ", 142, 1);
	print_sqrt("sqrt(1.1)      ", 11, 10);
	print_sqrt("sqrt(4.04)     ", 404, 100);
	print_sqrt("sqrt(1.000002) ", 1000002, 1000000);
	my_puts("\r\n");
}

void demo_lfsr(void){
	static lfsr_t l;
	static int ready = 0;
	int i, ones = 0;
	if (!ready){
		lfsr_init(&l, LFSR_TAPS_16_14_13_11, LFSR_SEED);
		ready = 1;
	}
	my_puts("bits  : ");
	for (i=0; i<64; i++) my_puts("%d", lfsr_bit(&l));
	my_puts("\r\n");
	my_puts("words : ");
	for (i=0; i<8; i++) my_puts("%u ", lfsr_word(&l));
	my_puts("\r\n");
	for (i=0; i<1024; i++) ones += lfsr_bit(&l);
	my_puts("ones in next 1024 bits = %d\r\n\r\n", ones);
}

int main(){
	clock_setup();
	init_gpio();
	usart_setup();

	while(1){
		blink_led();

		demo_fpa();
		demo_lfsr();

		delay(0xffffff);
	}
	return 0;
}

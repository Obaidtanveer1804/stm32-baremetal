#include <libopencm3/stm32/gpio.h>
#include "uart.h"
#include "cgpio.h"
#include "fixpoint.h"

// my_puts has no %06ld: printng the 6 decimals with their leading zeros
void print_fx(long a){
	long f = fx_frac(a);
	long d;
	if (a<0 && fx_int(a)==0) my_puts("-");      // -0.5 : int part is 0, sign lost
	my_puts("%d.",(int)fx_int(a));
	for (d=DECIMALES/10; d>1 && f<d; d/=10) my_puts("0");
	my_puts("%d",(int)f);
}

int main(){
	clock_setup();
	init_gpio();
	usart_setup();

	long r, c;
	while(1){
		blink_led();

		r = jmsqrt(2*DECIMALES*DECIMALES);          // sqrt(2), expect 1.414213
		my_puts("sqrt(2)    = "); print_fx(r); my_puts(" (1.414213)\r\n");

		r = jmsqrt(142*DECIMALES*DECIMALES);        // expect 11.916375
		my_puts("sqrt(142)  = "); print_fx(r); my_puts(" (11.916375)\r\n");

		c = fx_div(fx_from_int(3), fx_from_int(2)); // 1.5
		my_puts("3/2        = "); print_fx(c); my_puts("\r\n");

		c = fx_mul(c,c);                            // 2.25
		my_puts("1.5*1.5    = "); print_fx(c); my_puts("\r\n");

		c = fx_sqrt(c);                             // 1.5
		my_puts("sqrt(2.25) = "); print_fx(c); my_puts("\r\n\r\n");

		delay(0xfffff);
	}
	return 0;
}

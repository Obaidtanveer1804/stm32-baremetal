//fixpoint_test.c -- PC check of fixpoint library: gcc fixpoint_test.c fixpoint.c -o fixpoint_test
#include <stdio.h>
#include "fixpoint.h"

int main(void){
	// lab example: sqrt(2) with 6 decimals -> radicand 2*D*D
	long r=jmsqrt(2*DECIMALES*DECIMALES);
	printf("sqrt(2)   = %ld.%06ld (expect 1.414213)\n", fx_int(r), fx_frac(r));
	r=jmsqrt(142*DECIMALES*DECIMALES);
	printf("sqrt(142) = %ld.%06ld (expect 11.916375)\n", fx_int(r), fx_frac(r));

	long a=fx_from_int(3), b=fx_from_int(2);        // 3.0 and 2.0
	long c=fx_div(a,b);                              // 1.5
	printf("3/2       = %ld.%06ld\n", fx_int(c), fx_frac(c));
	c=fx_mul(c,c);                                   // 2.25
	printf("1.5*1.5   = %ld.%06ld\n", fx_int(c), fx_frac(c));
	c=fx_sqrt(c);                                    // 1.5
	printf("sqrt(2.25)= %ld.%06ld\n", fx_int(c), fx_frac(c));
	return 0;
}

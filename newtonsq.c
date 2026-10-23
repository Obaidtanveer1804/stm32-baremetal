#include "newtonsq.h"

uint32_t newtonsq(uint32_t S){

	if (S < 0 | S == 0){ return 0;}
	else{
		uint32_t x = S;
		uint32_t last_x = 0;
		while (last_x != x){
			last_x = x;
			x = (x + S/x)>>1; //shift by one is same as dividing by 2
		}
	return x;
	}

}

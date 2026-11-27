//fixpoint.c 
#include "fixpoint.h"

long jmsqrt(long long A){
	long long x, xp;
	if (A==0) return 0;
	if (A==1) return 1;            // Newton does not work for 1
	xp=A>>1;                       // initialisation: A/2
	do {
		x=xp;
		if (x!=0) xp=(x+A/x)>>1;   // x(n+1)=1/2*(x(n)+A/x(n))
		else break;
	} while ((x-xp)>1);
	return (long)xp;
}

//fixpoint.c -- fixed point arithmetic (solution of the previous lab, J-M Friedt)
#include "fixpoint.h"

// Newton: sqrt of an integer A  (solution from the lab slides)
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

long fx_from_int(long i){ return i*DECIMALES; }

long fx_mul(long a, long b){ return (long)(((long long)a*b)/DECIMALES); }

long fx_div(long a, long b){
	if (b==0) return 0;
	return (long)(((long long)a*DECIMALES)/b);
}

// sqrt(a/D)*D = sqrt(a*D)  -> the radicand is a*D (no more than a*D^2 needed here)
long fx_sqrt(long a){
	if (a<=0) return 0;
	return jmsqrt((long long)a*DECIMALES);
}

long fx_int(long a){ return a/DECIMALES; }

long fx_frac(long a){
	long f=a%DECIMALES;
	return (f<0)?-f:f;
}

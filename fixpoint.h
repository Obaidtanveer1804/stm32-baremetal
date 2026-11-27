//fixpoint.h 
#ifndef FIXPOINT_H
#define FIXPOINT_H

#define DECIMALES 1000000LL   // LL = long long (64 bit)

// integer square root by Newton: x(n+1) = (x(n) + A/x(n))/2
long jmsqrt(long long A);

#endif

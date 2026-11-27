//fixpoint.h -- fixed point arithmetic (solution of the previous lab, J-M Friedt)
#ifndef FIXPOINT_H
#define FIXPOINT_H

// a real number x is stored as the integer x*DECIMALES (here 6 decimals)
// ex: 1.414213 -> 1414213, so only integer maths is needed (no FPU on the STM32F1)
#define DECIMALES 1000000LL   // LL = long long (64 bit, avoids overflow of A*DECIMALES^2)

// integer square root by Newton: x(n+1) = (x(n) + A/x(n))/2
long jmsqrt(long long A);

// convert / combine fixed point numbers (value*DECIMALES)
long fx_from_int(long i);                    // 3 -> 3000000
long fx_mul(long a, long b);                 // (a*b)/DECIMALES
long fx_div(long a, long b);                 // (a*DECIMALES)/b
long fx_sqrt(long a);                        // sqrt of a fixed point number, result fixed point

// split a fixed point number for printing: printf("%ld.%06ld", fx_int(x), fx_frac(x))
long fx_int(long a);
long fx_frac(long a);                        // always >=0, 6 digits (use %06ld)

#endif

#include "fact.h"

int fact(int n)
{volatile int tmp=n; // on stack
 (void)tmp;
 if (n>2)
    return(fact(n-1)*n);
 else return(n);
}

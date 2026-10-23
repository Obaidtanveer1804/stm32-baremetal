#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fact.h"

int main(int argc, char** argv)
{static int val=5; 

while (1){
 if (argc>1) val=atoi(argv[1]);
 printf("resultat %d\n",fact(val));
 usleep(500000); // 500 ms
}
 return(0);
}

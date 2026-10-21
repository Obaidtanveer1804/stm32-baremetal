

#include <stdint.h>
#include <stdio.h>
#include "newtonsq.h"

uint32_t num = 4;
uint32_t sqroot = 0;

int main(void)
{
while(1){

sqroot = newtonsq(num);
printf("num = %u", num);
printf("sqroot %u \r\n",sqroot);
printf("hello\r\n");
num+=5;
}



return 0;
}

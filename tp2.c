#include "uart.h"
#include "tp2.h"
#include <stdint.h>

static const char hex_digits[] = "0123456789ABCDEF";
#define HEX_NIBBLE_BITS (4)
#define HEX_NIBBLE_MASK (0xFU)
#define BCOUNTER_BIT_COUNT (8U)
#define COUNTER_MAX_VALUE (255)

int counter(int count){
   if(count >= COUNTER_MAX_VALUE){count = 0;}else{count += 1;}
	my_puts("decimal count: ");
	my_putchar(hex_digits[count / 100U]);
	my_putchar(hex_digits[(count / 10U) % 10U]);
	my_putchar(hex_digits[count % 10U]);
	my_puts("\r\n");
	return count;
}

void tp2_task6_count_dec(int counter)
{
	my_puts("binary counter: ");
	for(uint8_t bit_index = BCOUNTER_BIT_COUNT; bit_index > 0U; bit_index--){
	my_puts(((counter >> (bit_index - 1U)) & 1U) ? "1" : "0");
	}
	my_puts("\r\n");
}

void tp2_task5_count_hex(int counter)
{
	my_puts("hex counter : 0x");
        my_putchar(hex_digits[(counter >> HEX_NIBBLE_BITS) & HEX_NIBBLE_MASK]);
        my_putchar(hex_digits[(counter & HEX_NIBBLE_MASK)]);
        my_puts("\r\n");
}

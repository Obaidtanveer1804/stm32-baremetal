#include "tp3.h"
#include "uart.h"
#include <stdint.h>

static const char hex_digits[] = "0123456789ABCDEF";
#define HEX_NIBBLE_BITS (4U)
#define HEX_NIBBLE_MASK (0xFU)
#define WORD_NIBBLE_COUNT (4U)

#define AD_DIVISOR (546875U)
#define AD_SHIFT (21U)
#define AD_SHIFT_A (10U)
#define AD_SHIFT_B (11U)
#define AD_MASK_14BITS (0x3FFFU)
#define AD_FREQ0_ADDR (0x4000U)
#define AD_CONTROL_B28 (0x2000U)

static void print_hex16(uint16_t word)
{
	my_puts("0x");
	for(uint8_t nibble = WORD_NIBBLE_COUNT; nibble > 0U; nibble--){
		my_putchar(hex_digits[(word >> ((nibble - 1U) * HEX_NIBBLE_BITS)) & HEX_NIBBLE_MASK]);
	}
}

static uint32_t ad_freqreg(uint32_t req_freq)
{
	uint32_t whole = req_freq / AD_DIVISOR;
	uint32_t step1 = (req_freq % AD_DIVISOR) << AD_SHIFT_A;
	uint32_t step2 = (step1 % AD_DIVISOR) << AD_SHIFT_B;

	return (whole << AD_SHIFT) + ((step1 / AD_DIVISOR) << AD_SHIFT_B) + (step2 / AD_DIVISOR);
}

int ad_freq(int req_freq){
	uint32_t freqreg = ad_freqreg((uint32_t)req_freq);
	uint16_t word_lsb = AD_FREQ0_ADDR | (freqreg & AD_MASK_14BITS);
	uint16_t word_msb = AD_FREQ0_ADDR | ((freqreg >> 14) & AD_MASK_14BITS);

	my_puts("control word: ");
	print_hex16(AD_CONTROL_B28);
	my_puts("\r\nfirst word (14 LSB): ");
	print_hex16(word_lsb);
	my_puts("\r\nsecond word (14 MSB): ");
	print_hex16(word_msb);
	my_puts("\r\n");
	return (int)freqreg;
}

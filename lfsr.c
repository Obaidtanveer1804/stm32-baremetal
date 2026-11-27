#include "lfsr.h"

void lfsr_init(lfsr_t *l, uint16_t taps, uint16_t seed){
	l->taps = taps;
	l->state = (seed==0) ? LFSR_SEED : seed;
}

int lfsr_bit(lfsr_t *l){
	uint16_t x = l->state & l->taps;
	uint16_t bit;
	x ^= x >> 8;
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	bit = x & 1;
	l->state = (l->state >> 1) | (bit << 15);
	return bit;
}

uint16_t lfsr_word(lfsr_t *l){
	uint16_t w = 0;
	int i;
	for (i=0; i<16; i++) w = (w << 1) | lfsr_bit(l);
	return w;
}

uint32_t lfsr_period(uint16_t taps){
	lfsr_t l;
	uint32_t period = 0;
	lfsr_init(&l, taps, LFSR_SEED);
	do {
		lfsr_bit(&l);
		period++;
	} while (l.state != LFSR_SEED && period < 70000);
	return period;
}

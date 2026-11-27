#ifndef LFSR_H
#define LFSR_H

#include <stdint.h>

#define LFSR_SEED      0xACE1u

#define LFSR_TAPS_16_14_13_11   0x002Du
#define LFSR_TAPS_16_15_13_4    0x100Bu

typedef struct {
	uint16_t state;
	uint16_t taps;
} lfsr_t;

void lfsr_init(lfsr_t *l, uint16_t taps, uint16_t seed);
int lfsr_bit(lfsr_t *l);
uint16_t lfsr_word(lfsr_t *l);
uint32_t lfsr_period(uint16_t taps);

#endif

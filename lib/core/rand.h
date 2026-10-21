/**
 * @file    rand.h
 * @brief   Small pseudo-random generators (not cryptographic).
 *
 * - xorshift32 : fast, 32-bit state, period 2^32-1
 * - LFSR16     : Galois LFSR, the classic hardware shift-register PRBS
 * - range / shuffle helpers without modulo bias
 *
 * Seed from something noisy on the target: the ADC LSBs of a floating
 * pin, the internal temperature sensor, or the timer count when the
 * user first presses a button.
 */
#ifndef RAND_H
#define RAND_H

#include <stdint.h>

typedef struct {
    uint32_t state;
} rand_xs32_t;

void     rand_xs32_seed(rand_xs32_t *r, uint32_t seed);
uint32_t rand_xs32_next(rand_xs32_t *r);
/** Uniform in [0, bound) without modulo bias (Lemire's method). */
uint32_t rand_xs32_below(rand_xs32_t *r, uint32_t bound);
/** Uniform in [lo, hi] inclusive. */
int32_t  rand_xs32_range(rand_xs32_t *r, int32_t lo, int32_t hi);
/** Fisher-Yates shuffle of a byte array. */
void     rand_shuffle_u8(rand_xs32_t *r, uint8_t *array, uint32_t len);

/** One step of a 16-bit Galois LFSR (taps 16,14,13,11: 0xB400). */
uint16_t rand_lfsr16_next(uint16_t *state);
/** Output one PRBS bit (e.g. to drive a GPIO as a noise source). */
uint8_t  rand_lfsr16_bit(uint16_t *state);

/** Global convenience generator. */
void     rand_seed(uint32_t seed);
uint32_t rand_u32(void);

#endif /* RAND_H */

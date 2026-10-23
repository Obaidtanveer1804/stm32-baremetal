/**
 * @file    rand.c
 * @brief   Pseudo-random generators (see rand.h).
 */
#include "rand.h"

#define LFSR16_TAPS   (0xB400U)
#define DEFAULT_SEED  (0x2545F491UL)

static rand_xs32_t s_global = { DEFAULT_SEED };

void rand_xs32_seed(rand_xs32_t *r, uint32_t seed)
{
    /* xorshift must never be in state 0 (it would stay 0 forever) */
    r->state = (seed != 0U) ? seed : DEFAULT_SEED;
}

uint32_t rand_xs32_next(rand_xs32_t *r)
{
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return x;
}

uint32_t rand_xs32_below(rand_xs32_t *r, uint32_t bound)
{
    if (bound == 0U) {
        return 0U;
    }
    /* multiply-shift maps 32 random bits to [0, bound); reject the
     * small biased zone at the low end of the product */
    uint64_t m         = (uint64_t)rand_xs32_next(r) * bound;
    uint32_t low       = (uint32_t)m;
    if (low < bound) {
        const uint32_t threshold = (0U - bound) % bound;
        while (low < threshold) {
            m   = (uint64_t)rand_xs32_next(r) * bound;
            low = (uint32_t)m;
        }
    }
    return (uint32_t)(m >> 32);
}

int32_t rand_xs32_range(rand_xs32_t *r, int32_t lo, int32_t hi)
{
    if (hi < lo) {
        const int32_t t = lo;
        lo = hi;
        hi = t;
    }
    const uint32_t span = (uint32_t)hi - (uint32_t)lo + 1U;
    if (span == 0U) {                     /* full 32-bit range */
        return (int32_t)rand_xs32_next(r);
    }
    return (int32_t)((uint32_t)lo + rand_xs32_below(r, span));
}

void rand_shuffle_u8(rand_xs32_t *r, uint8_t *array, uint32_t len)
{
    for (uint32_t i = len; i > 1U; i--) {
        const uint32_t j   = rand_xs32_below(r, i);
        const uint8_t  tmp = array[i - 1U];
        array[i - 1U] = array[j];
        array[j]      = tmp;
    }
}

uint16_t rand_lfsr16_next(uint16_t *state)
{
    uint16_t s = (*state != 0U) ? *state : 0xACE1U;
    const uint16_t lsb = s & 1U;
    s >>= 1;
    if (lsb != 0U) {
        s ^= LFSR16_TAPS;
    }
    *state = s;
    return s;
}

uint8_t rand_lfsr16_bit(uint16_t *state)
{
    return (uint8_t)(rand_lfsr16_next(state) & 1U);
}

void rand_seed(uint32_t seed)
{
    rand_xs32_seed(&s_global, seed);
}

uint32_t rand_u32(void)
{
    return rand_xs32_next(&s_global);
}

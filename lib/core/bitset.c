/**
 * @file    bitset.c
 * @brief   Bit arrays (see bitset.h).
 */
#include "bitset.h"

/* mask of the valid bits in the last word */
static uint32_t tail_mask(uint32_t nbits)
{
    const uint32_t r = nbits & 31U;
    return (r == 0U) ? 0xFFFFFFFFU : ((1U << r) - 1U);
}

void bitset_fill(uint32_t *bs, uint32_t nbits, bool value)
{
    const uint32_t words = BITSET_WORDS(nbits);
    for (uint32_t w = 0U; w < words; w++) {
        bs[w] = value ? 0xFFFFFFFFU : 0U;
    }
    if (value && (words > 0U)) {
        bs[words - 1U] &= tail_mask(nbits);     /* keep unused bits clear */
    }
}

uint32_t bitset_count(const uint32_t *bs, uint32_t nbits)
{
    const uint32_t words = BITSET_WORDS(nbits);
    uint32_t       n     = 0U;
    for (uint32_t w = 0U; w < words; w++) {
        const uint32_t v = (w == (words - 1U)) ? (bs[w] & tail_mask(nbits)) : bs[w];
        n += bits_popcount(v);
    }
    return n;
}

int32_t bitset_find_next_set(const uint32_t *bs, uint32_t nbits, uint32_t from)
{
    if (from >= nbits) {
        return -1;
    }
    const uint32_t words = BITSET_WORDS(nbits);
    uint32_t       w     = from >> 5;
    /* first word: ignore bits below "from" */
    uint32_t v = bs[w] & (0xFFFFFFFFU << (from & 31U));
    for (;;) {
        if (w == (words - 1U)) {
            v &= tail_mask(nbits);
        }
        if (v != 0U) {
            return (int32_t)((w << 5) + bits_ctz(v));
        }
        if (++w >= words) {
            return -1;
        }
        v = bs[w];
    }
}

int32_t bitset_find_first_set(const uint32_t *bs, uint32_t nbits)
{
    return bitset_find_next_set(bs, nbits, 0U);
}

int32_t bitset_find_first_zero(const uint32_t *bs, uint32_t nbits)
{
    const uint32_t words = BITSET_WORDS(nbits);
    for (uint32_t w = 0U; w < words; w++) {
        uint32_t inv = ~bs[w];
        if (w == (words - 1U)) {
            inv &= tail_mask(nbits);
        }
        if (inv != 0U) {
            return (int32_t)((w << 5) + bits_ctz(inv));
        }
    }
    return -1;
}

int32_t bitset_alloc(uint32_t *bs, uint32_t nbits)
{
    const int32_t i = bitset_find_first_zero(bs, nbits);
    if (i >= 0) {
        bitset_set(bs, (uint32_t)i);
    }
    return i;
}

/**
 * @file    bitset.h
 * @brief   Arbitrary-size bit arrays on uint32_t words (flags, slot
 *          allocation, LED matrix rows, sensor presence maps...).
 *
 *   BITSET_DEFINE(used_slots, 100);
 *   bitset_set(used_slots, 42);
 *   int32_t free = bitset_find_first_zero(used_slots, 100);
 */
#ifndef BITSET_H
#define BITSET_H

#include <stdint.h>
#include <stdbool.h>
#include "bits.h"

#define BITSET_WORDS(nbits)          (((nbits) + 31U) / 32U)
#define BITSET_DEFINE(name, nbits)   uint32_t name[BITSET_WORDS(nbits)] = { 0U }

static inline void bitset_set(uint32_t *bs, uint32_t i)    { bs[i >> 5] |=  (1U << (i & 31U)); }
static inline void bitset_clear(uint32_t *bs, uint32_t i)  { bs[i >> 5] &= ~(1U << (i & 31U)); }
static inline void bitset_toggle(uint32_t *bs, uint32_t i) { bs[i >> 5] ^=  (1U << (i & 31U)); }
static inline bool bitset_test(const uint32_t *bs, uint32_t i)
{
    return (bs[i >> 5] & (1U << (i & 31U))) != 0U;
}

void     bitset_fill(uint32_t *bs, uint32_t nbits, bool value);
uint32_t bitset_count(const uint32_t *bs, uint32_t nbits);
/** Index of the first set / clear bit, -1 if none. Skips whole words. */
int32_t  bitset_find_first_set(const uint32_t *bs, uint32_t nbits);
int32_t  bitset_find_first_zero(const uint32_t *bs, uint32_t nbits);
/** Next set bit at or after @p from, -1 if none (iterate set bits fast). */
int32_t  bitset_find_next_set(const uint32_t *bs, uint32_t nbits, uint32_t from);
/** Find a clear bit, set it, return it (-1 if full): a slot allocator. */
int32_t  bitset_alloc(uint32_t *bs, uint32_t nbits);

#endif /* BITSET_H */

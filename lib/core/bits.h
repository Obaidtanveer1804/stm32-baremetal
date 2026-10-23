/**
 * @file    bits.h
 * @brief   Bit-manipulation helpers (masks, fields, tricks).
 *
 * Header-only, no dependencies beyond <stdint.h>. Every macro is fully
 * parenthesised and works on unsigned operands; prefer the inline
 * functions when an argument has side effects.
 *
 * Portable: builds on the MCU and on the PC.
 */
#ifndef BITS_H
#define BITS_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Single bits and masks                                               */
/* ------------------------------------------------------------------ */

/** Mask with only bit @p n set (n in 0..31). */
#define BIT(n)                  ((uint32_t)1U << (n))

/** Mask with the @p width lowest bits set (width in 0..32). */
#define BIT_MASK(width)         (((width) >= 32U) ? 0xFFFFFFFFU : (BIT(width) - 1U))

/** Mask of @p width bits starting at bit @p pos. */
#define FIELD_MASK(pos, width)  (BIT_MASK(width) << (pos))

#define BIT_SET(reg, mask)      ((reg) |=  (mask))
#define BIT_CLR(reg, mask)      ((reg) &= ~(mask))
#define BIT_TGL(reg, mask)      ((reg) ^=  (mask))
#define BIT_IS_SET(reg, mask)   (((reg) & (mask)) == (mask))
#define BIT_ANY_SET(reg, mask)  (((reg) & (mask)) != 0U)

/** Read-modify-write: clear @p clr_mask then set @p set_mask. */
#define BIT_MODIFY(reg, clr_mask, set_mask) \
    ((reg) = (((reg) & ~(clr_mask)) | (set_mask)))

/** Extract a bit field. */
#define FIELD_GET(reg, pos, width) \
    (((reg) >> (pos)) & BIT_MASK(width))

/** Write a bit field without touching the other bits. */
#define FIELD_SET(reg, pos, width, val) \
    BIT_MODIFY((reg), FIELD_MASK((pos), (width)), \
               (((uint32_t)(val) & BIT_MASK(width)) << (pos)))

/** Number of elements of a static array. */
#define ARRAY_SIZE(a)           (sizeof(a) / sizeof((a)[0]))

/** Compile-time assertion (C99 compatible). */
#define STATIC_ASSERT(cond, name) \
    typedef char static_assert_##name[(cond) ? 1 : -1]

#define MIN(a, b)               (((a) < (b)) ? (a) : (b))
#define MAX(a, b)               (((a) > (b)) ? (a) : (b))
#define CLAMP(x, lo, hi)        (MIN(MAX((x), (lo)), (hi)))
#define UNUSED(x)               ((void)(x))

/* ------------------------------------------------------------------ */
/* Power-of-two tricks                                                 */
/* ------------------------------------------------------------------ */

/** True if @p x is a power of two (0 is not). */
#define IS_POW2(x)              (((x) != 0U) && (((x) & ((x) - 1U)) == 0U))

/** Round @p x up to a multiple of @p align (align must be a power of 2). */
#define ALIGN_UP(x, align)      (((x) + ((align) - 1U)) & ~((align) - 1U))
#define ALIGN_DOWN(x, align)    ((x) & ~((align) - 1U))

/** x % m for m power of two, without a division. */
#define MOD_POW2(x, m)          ((x) & ((m) - 1U))

/* ------------------------------------------------------------------ */
/* Inline helpers                                                      */
/* ------------------------------------------------------------------ */

/** Count set bits (Hamming weight), SWAR method, no table, no loop. */
static inline uint32_t bits_popcount(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555U);
    x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
    x = (x + (x >> 4)) & 0x0F0F0F0FU;
    return (x * 0x01010101U) >> 24;
}

/** Even parity: 1 if an odd number of bits are set. */
static inline uint32_t bits_parity(uint32_t x)
{
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    return (0x6996U >> (x & 0xFU)) & 1U;
}

/** Reverse the bit order of a byte (b7..b0 -> b0..b7). */
static inline uint8_t bits_reverse8(uint8_t b)
{
    b = (uint8_t)(((b & 0xF0U) >> 4) | ((b & 0x0FU) << 4));
    b = (uint8_t)(((b & 0xCCU) >> 2) | ((b & 0x33U) << 2));
    b = (uint8_t)(((b & 0xAAU) >> 1) | ((b & 0x55U) << 1));
    return b;
}

/** Reverse the bit order of a 32-bit word. */
static inline uint32_t bits_reverse32(uint32_t x)
{
    x = ((x >> 1) & 0x55555555U) | ((x & 0x55555555U) << 1);
    x = ((x >> 2) & 0x33333333U) | ((x & 0x33333333U) << 2);
    x = ((x >> 4) & 0x0F0F0F0FU) | ((x & 0x0F0F0F0FU) << 4);
    x = ((x >> 8) & 0x00FF00FFU) | ((x & 0x00FF00FFU) << 8);
    return (x >> 16) | (x << 16);
}

/** Count leading zeros (returns 32 for x == 0). */
static inline uint32_t bits_clz(uint32_t x)
{
    uint32_t n = 0U;
    if (x == 0U) {
        return 32U;
    }
    if ((x & 0xFFFF0000U) == 0U) { n += 16U; x <<= 16; }
    if ((x & 0xFF000000U) == 0U) { n += 8U;  x <<= 8;  }
    if ((x & 0xF0000000U) == 0U) { n += 4U;  x <<= 4;  }
    if ((x & 0xC0000000U) == 0U) { n += 2U;  x <<= 2;  }
    if ((x & 0x80000000U) == 0U) { n += 1U; }
    return n;
}

/** Count trailing zeros (returns 32 for x == 0). */
static inline uint32_t bits_ctz(uint32_t x)
{
    if (x == 0U) {
        return 32U;
    }
    /* isolate the lowest set bit, then its index = 31 - clz */
    return 31U - bits_clz(x & (~x + 1U));
}

/** Index of the most significant set bit: floor(log2(x)); -1 for 0. */
static inline int32_t bits_msb_index(uint32_t x)
{
    return (x == 0U) ? -1 : (int32_t)(31U - bits_clz(x));
}

/** Clear the lowest set bit (x & (x - 1)). */
static inline uint32_t bits_clear_lowest(uint32_t x)
{
    return x & (x - 1U);
}

/** Keep only the lowest set bit (x & -x). */
static inline uint32_t bits_lowest(uint32_t x)
{
    return x & (~x + 1U);
}

/** Smallest power of two >= x (x = 0 returns 1, x > 2^31 returns 0). */
static inline uint32_t bits_next_pow2(uint32_t x)
{
    if (x <= 1U) {
        return 1U;
    }
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1U;
}

/** Rotate left / right. */
static inline uint32_t bits_rotl32(uint32_t x, uint32_t n)
{
    n &= 31U;
    return (n == 0U) ? x : ((x << n) | (x >> (32U - n)));
}

static inline uint32_t bits_rotr32(uint32_t x, uint32_t n)
{
    n &= 31U;
    return (n == 0U) ? x : ((x >> n) | (x << (32U - n)));
}

/** Byte swap (endianness change). */
static inline uint16_t bits_bswap16(uint16_t x)
{
    return (uint16_t)((x << 8) | (x >> 8));
}

static inline uint32_t bits_bswap32(uint32_t x)
{
    return ((x & 0x000000FFU) << 24) | ((x & 0x0000FF00U) << 8) |
           ((x & 0x00FF0000U) >> 8)  | ((x & 0xFF000000U) >> 24);
}

/** Sign-extend the @p width lowest bits of @p x to 32 bits. */
static inline int32_t bits_sign_extend(uint32_t x, uint32_t width)
{
    const uint32_t m = (uint32_t)1U << (width - 1U);
    x &= BIT_MASK(width);
    return (int32_t)((x ^ m) - m);
}

/** Branch-free absolute value of a signed 32-bit integer. */
static inline uint32_t bits_abs32(int32_t v)
{
    const uint32_t mask = (uint32_t)(v >> 31);
    return ((uint32_t)v ^ mask) - mask;
}

/** True when @p a and @p b have opposite signs. */
static inline bool bits_opposite_sign(int32_t a, int32_t b)
{
    return (a ^ b) < 0;
}

/** Swap two variables with XOR (no temporary). Pointers must differ. */
static inline void bits_xor_swap(uint32_t *a, uint32_t *b)
{
    if (a != b) {
        *a ^= *b;
        *b ^= *a;
        *a ^= *b;
    }
}

/** Spread the 16 low bits of x to even positions (Morton/Z-order). */
static inline uint32_t bits_spread16(uint32_t x)
{
    x &= 0x0000FFFFU;
    x = (x | (x << 8)) & 0x00FF00FFU;
    x = (x | (x << 4)) & 0x0F0F0F0FU;
    x = (x | (x << 2)) & 0x33333333U;
    x = (x | (x << 1)) & 0x55555555U;
    return x;
}

/** Interleave x and y bits: Morton code, useful for 2D LED matrices. */
static inline uint32_t bits_morton2(uint16_t x, uint16_t y)
{
    return bits_spread16(x) | (bits_spread16(y) << 1);
}

#endif /* BITS_H */

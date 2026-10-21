/**
 * @file    fixmath.h
 * @brief   Integer and fixed-point math: no FPU, no libm.
 *
 * The STM32F1 (Cortex-M3) has no floating-point unit; software floats
 * cost ~10 kB of flash and hundreds of cycles per operation. These
 * functions replace them with integer arithmetic.
 *
 *  q16_t : signed Q16.16 (range +-32768, resolution 1.5e-5)
 *  q15_t : signed Q1.15  (range [-1, 1), DSP style, for filters/audio)
 *
 * Angles for the trig functions are in "binary radians" (brad):
 * a full turn is 65536, so the angle wraps for free in a uint16_t.
 */
#ifndef FIXMATH_H
#define FIXMATH_H

#include <stdint.h>
#include <stdbool.h>

typedef int32_t q16_t;
typedef int16_t q15_t;

#define Q16_ONE        ((q16_t)0x00010000L)
#define Q16_HALF       ((q16_t)0x00008000L)
#define Q16_MAX        ((q16_t)0x7FFFFFFFL)
#define Q16_MIN        ((q16_t)0x80000000L)
#define Q16_PI         ((q16_t)205887L)          /* 3.14159 * 65536 */
#define Q15_ONE        ((q15_t)0x7FFF)           /* closest to 1.0  */

/** Build constants at compile time: Q16_FROM_INT(3), Q16_CONST(1.5) */
#define Q16_FROM_INT(i)  ((q16_t)((int32_t)(i) * 65536L))
#define Q16_CONST(f)     ((q16_t)((f) * 65536.0 + (((f) >= 0) ? 0.5 : -0.5)))
#define Q15_CONST(f)     ((q15_t)((f) * 32768.0 + (((f) >= 0) ? 0.5 : -0.5)))

#define BRAD_FULL_TURN   (65536UL)
#define BRAD_FROM_DEG(d) ((uint16_t)(((int32_t)(d) * 65536L) / 360L))

/* -------------------------- Q16.16 ops ---------------------------- */
static inline int32_t q16_to_int(q16_t a)        { return a >> 16; }   /* floor */
static inline int32_t q16_round(q16_t a)         { return (a + Q16_HALF) >> 16; }
static inline q16_t   q16_add(q16_t a, q16_t b)  { return a + b; }
static inline q16_t   q16_sub(q16_t a, q16_t b)  { return a - b; }
q16_t   q16_mul(q16_t a, q16_t b);                /**< rounded             */
q16_t   q16_div(q16_t a, q16_t b);                /**< saturates on /0     */
q16_t   q16_add_sat(q16_t a, q16_t b);
q16_t   q16_sqrt(q16_t a);                        /**< 0 for negatives     */
q16_t   q16_from_ratio(int32_t num, int32_t den);
q16_t   q16_lerp(q16_t a, q16_t b, q16_t t);      /**< t in [0, 1]          */
/** Signed value of a sine/cosine in Q16, angle in brad (CORDIC). */
q16_t   q16_sin(uint16_t angle);
q16_t   q16_cos(uint16_t angle);
/** atan2 in brad (0..65535), CORDIC vectoring mode. */
uint16_t q16_atan2(int32_t y, int32_t x);
/** Vector magnitude sqrt(x^2+y^2), rounded, no overflow, same unit as x/y. */
uint32_t fx_hypot(int32_t x, int32_t y);

/* --------------------------- Q1.15 ops ---------------------------- */
q15_t   q15_mul(q15_t a, q15_t b);
q15_t   q15_add_sat(q15_t a, q15_t b);
q15_t   q15_sub_sat(q15_t a, q15_t b);

/* --------------------------- integer ------------------------------ */
/** floor(sqrt(x)), bit-by-bit, no division. */
uint32_t fx_isqrt32(uint32_t x);
uint32_t fx_isqrt64(uint64_t x);
/** floor(log2(x)); returns 0 for x = 0. */
uint32_t fx_ilog2(uint32_t x);
/** base^exp with overflow saturating at UINT32_MAX. */
uint32_t fx_ipow(uint32_t base, uint32_t exp);
uint32_t fx_gcd(uint32_t a, uint32_t b);
uint32_t fx_lcm(uint32_t a, uint32_t b);
/** Rounded division (a + b/2) / b, correct for negative numbers too. */
int32_t  fx_div_round(int32_t a, int32_t b);
/** Arduino-style range mapping, 64-bit intermediate (no overflow). */
int32_t  fx_map(int32_t x, int32_t in_min, int32_t in_max,
                int32_t out_min, int32_t out_max);
int32_t  fx_clamp(int32_t x, int32_t lo, int32_t hi);
int32_t  fx_add_sat(int32_t a, int32_t b);
int32_t  fx_sub_sat(int32_t a, int32_t b);
uint32_t fx_uadd_sat(uint32_t a, uint32_t b);
int32_t  fx_abs(int32_t a);
int32_t  fx_sign(int32_t a);
/** Divide by 10 with multiply+shift (what compilers do), exact for u32. */
uint32_t fx_div10(uint32_t x);

/* ------------------- integer trig via table ----------------------- */
/** Sine from a 65-entry quarter-wave table + linear interpolation,
 *  result in Q15 (-32767..32767). Faster than CORDIC. */
q15_t    fx_sin_q15(uint16_t angle);
q15_t    fx_cos_q15(uint16_t angle);

#endif /* FIXMATH_H */

/**
 * @file    fixmath.c
 * @brief   Integer / fixed-point math (see fixmath.h).
 */
#include "fixmath.h"

/* ------------------------------------------------------------------ */
/* Integer helpers                                                     */
/* ------------------------------------------------------------------ */

uint32_t fx_isqrt32(uint32_t x)
{
    uint32_t root = 0U;
    uint32_t bit  = (uint32_t)1U << 30;     /* highest power of 4 <= 2^32 */

    while (bit > x) {
        bit >>= 2;
    }
    while (bit != 0U) {
        if (x >= (root + bit)) {
            x    -= root + bit;
            root  = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

uint32_t fx_isqrt64(uint64_t x)
{
    uint64_t root = 0U;
    uint64_t bit  = 1ULL << 62;

    while (bit > x) {
        bit >>= 2;
    }
    while (bit != 0U) {
        if (x >= (root + bit)) {
            x    -= root + bit;
            root  = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)root;
}

uint32_t fx_ilog2(uint32_t x)
{
    uint32_t r = 0U;
    while (x > 1U) {
        x >>= 1;
        r++;
    }
    return r;
}

uint32_t fx_ipow(uint32_t base, uint32_t exp)
{
    uint64_t result = 1U;
    uint64_t b      = base;

    /* exponentiation by squaring: O(log exp) multiplications */
    while (exp != 0U) {
        if ((exp & 1U) != 0U) {
            result *= b;
            if (result > 0xFFFFFFFFULL) {
                return 0xFFFFFFFFUL;
            }
        }
        exp >>= 1;
        if (exp != 0U) {
            b *= b;
            if (b > 0xFFFFFFFFULL) {
                b = 0x100000000ULL;   /* any further use overflows */
            }
        }
    }
    return (uint32_t)result;
}

uint32_t fx_gcd(uint32_t a, uint32_t b)
{
    while (b != 0U) {
        const uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

uint32_t fx_lcm(uint32_t a, uint32_t b)
{
    if ((a == 0U) || (b == 0U)) {
        return 0U;
    }
    return (a / fx_gcd(a, b)) * b;
}

int32_t fx_div_round(int32_t a, int32_t b)
{
    if (b == 0) {
        return (a >= 0) ? INT32_MAX : INT32_MIN;
    }
    const int64_t num  = a;
    const int64_t den  = b;
    const int64_t half = (den > 0) ? (den / 2) : (-den / 2);
    /* round half away from zero */
    return (int32_t)(((num < 0) == (den < 0)) ? ((num + ((den > 0) ? half : -half)) / den)
                                              : ((num - ((den > 0) ? half : -half)) / den));
}

int32_t fx_map(int32_t x, int32_t in_min, int32_t in_max,
               int32_t out_min, int32_t out_max)
{
    if (in_max == in_min) {
        return out_min;
    }
    const int64_t num = (int64_t)(x - in_min) * (int64_t)(out_max - out_min);
    return (int32_t)((num / (int64_t)(in_max - in_min)) + out_min);
}

int32_t fx_clamp(int32_t x, int32_t lo, int32_t hi)
{
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

int32_t fx_add_sat(int32_t a, int32_t b)
{
    const int64_t s = (int64_t)a + b;
    return (s > INT32_MAX) ? INT32_MAX : ((s < INT32_MIN) ? INT32_MIN : (int32_t)s);
}

int32_t fx_sub_sat(int32_t a, int32_t b)
{
    const int64_t s = (int64_t)a - b;
    return (s > INT32_MAX) ? INT32_MAX : ((s < INT32_MIN) ? INT32_MIN : (int32_t)s);
}

uint32_t fx_uadd_sat(uint32_t a, uint32_t b)
{
    const uint32_t s = a + b;
    /* carry happened if the sum wrapped below an operand */
    return (s < a) ? 0xFFFFFFFFUL : s;
}

int32_t fx_abs(int32_t a)
{
    return (a == INT32_MIN) ? INT32_MAX : ((a < 0) ? -a : a);
}

int32_t fx_sign(int32_t a)
{
    return (a > 0) - (a < 0);
}

uint32_t fx_div10(uint32_t x)
{
    /* 0xCCCCCCCD / 2^35 == 1/10 with enough precision for all 32-bit x */
    return (uint32_t)(((uint64_t)x * 0xCCCCCCCDULL) >> 35);
}

/* ------------------------------------------------------------------ */
/* Q16.16                                                              */
/* ------------------------------------------------------------------ */

static q16_t q16_saturate64(int64_t v)
{
    return (v > INT32_MAX) ? Q16_MAX : ((v < INT32_MIN) ? Q16_MIN : (q16_t)v);
}

q16_t q16_mul(q16_t a, q16_t b)
{
    const int64_t p = ((int64_t)a * b) + Q16_HALF;
    return q16_saturate64(p >> 16);
}

q16_t q16_div(q16_t a, q16_t b)
{
    if (b == 0) {
        return (a >= 0) ? Q16_MAX : Q16_MIN;
    }
    int64_t num = (int64_t)a * 65536;
    /* rounding: add half the divisor with the sign of the result */
    num += (((num >= 0) == (b > 0)) ? 1 : -1) * ((int64_t)((b > 0) ? b : -b) / 2);
    return q16_saturate64(num / b);
}

q16_t q16_add_sat(q16_t a, q16_t b)
{
    return fx_add_sat(a, b);
}

q16_t q16_sqrt(q16_t a)
{
    if (a <= 0) {
        return 0;
    }
    /* sqrt(a / 2^16) * 2^16 == sqrt(a * 2^16), rounded to nearest:
     * r is floor(sqrt(n)); round up when n - r^2 > r (i.e. n > (r+0.5)^2) */
    const uint64_t n = (uint64_t)a << 16;
    uint64_t       r = fx_isqrt64(n);
    if ((n - (r * r)) > r) {
        r++;
    }
    return (q16_t)r;
}

q16_t q16_from_ratio(int32_t num, int32_t den)
{
    /* (num << 16) / den is exactly what q16_div computes */
    return q16_div(num, den);
}

q16_t q16_lerp(q16_t a, q16_t b, q16_t t)
{
    return a + q16_mul(b - a, t);
}

/* -------------------------- CORDIC -------------------------------- */
/* Angles in "turn * 2^30" units (2^30 = full turn) for extra precision. */

#define CORDIC_ITERATIONS   (24U)
#define CORDIC_TURN         ((int32_t)1 << 30)
#define CORDIC_HALF_TURN    ((int32_t)1 << 29)
#define CORDIC_QUARTER_TURN ((int32_t)1 << 28)
#define CORDIC_GAIN_Q30     (652032874L)   /* 1/K = 0.607252935 * 2^30 */

static const int32_t cordic_atan_table[CORDIC_ITERATIONS] = {
    134217728L, 79233351L, 41864727L, 21251189L, 10666833L, 5338616L,
    2669960L,   1335061L,  667541L,   333772L,   166886L,   83443L,
    41722L,     20861L,    10430L,    5215L,     2608L,     1304L,
    652L,       326L,      163L,      81L,       41L,       20L
};

/* Rotation mode: returns cos and sin in Q30. */
static void cordic_rotate(uint16_t angle, int32_t *cos_q30, int32_t *sin_q30)
{
    int32_t z      = (int32_t)((uint32_t)angle << 14);   /* 0 .. 2^30 */
    int32_t x      = CORDIC_GAIN_Q30;
    int32_t y      = 0;
    bool    negate = false;

    if (z >= CORDIC_HALF_TURN) {
        z -= CORDIC_TURN;                  /* now in [-1/2, 1/2) turn */
    }
    /* CORDIC converges for |z| <= ~99 deg: fold into [-90, 90] */
    if (z > CORDIC_QUARTER_TURN) {
        z     -= CORDIC_HALF_TURN;
        negate = true;
    } else if (z < -CORDIC_QUARTER_TURN) {
        z     += CORDIC_HALF_TURN;
        negate = true;
    } else {
        /* already in range */
    }

    for (uint32_t i = 0U; i < CORDIC_ITERATIONS; i++) {
        const int32_t xs = x >> i;
        const int32_t ys = y >> i;
        if (z >= 0) {
            x -= ys;
            y += xs;
            z -= cordic_atan_table[i];
        } else {
            x += ys;
            y -= xs;
            z += cordic_atan_table[i];
        }
    }
    *cos_q30 = negate ? -x : x;
    *sin_q30 = negate ? -y : y;
}

q16_t q16_sin(uint16_t angle)
{
    int32_t c;
    int32_t s;
    cordic_rotate(angle, &c, &s);
    return (s + ((int32_t)1 << 13)) >> 14;     /* Q30 -> Q16, rounded */
}

q16_t q16_cos(uint16_t angle)
{
    int32_t c;
    int32_t s;
    cordic_rotate(angle, &c, &s);
    return (c + ((int32_t)1 << 13)) >> 14;
}

uint16_t q16_atan2(int32_t y, int32_t x)
{
    int64_t xx = x;
    int64_t yy = y;
    int64_t z  = 0;

    if ((x == 0) && (y == 0)) {
        return 0U;
    }
    /* scale small vectors up so the shifts below keep enough bits */
    while ((xx < ((int64_t)1 << 40)) && (xx > -((int64_t)1 << 40)) &&
           (yy < ((int64_t)1 << 40)) && (yy > -((int64_t)1 << 40))) {
        xx *= 2;
        yy *= 2;
    }
    /* vectoring converges for x > 0: rotate left half-plane by 180 deg */
    if (xx < 0) {
        xx = -xx;
        yy = -yy;
        z  = CORDIC_HALF_TURN;
    }
    for (uint32_t i = 0U; i < CORDIC_ITERATIONS; i++) {
        const int64_t xs = xx >> i;
        const int64_t ys = yy >> i;
        if (yy > 0) {
            xx += ys;
            yy -= xs;
            z  += cordic_atan_table[i];
        } else {
            xx -= ys;
            yy += xs;
            z  -= cordic_atan_table[i];
        }
    }
    /* back to brad with rounding; uint16 cast wraps negative angles */
    return (uint16_t)((uint32_t)((z + ((int64_t)1 << 13)) >> 14) & 0xFFFFU);
}

uint32_t fx_hypot(int32_t x, int32_t y)
{
    const int64_t  xx = x;
    const int64_t  yy = y;
    const uint64_t s  = (uint64_t)((xx * xx) + (yy * yy));
    uint64_t       r  = fx_isqrt64(s);
    if ((s - (r * r)) > r) {
        r++;                      /* round to nearest instead of floor */
    }
    return (uint32_t)r;
}

/* ------------------------------------------------------------------ */
/* Q1.15                                                               */
/* ------------------------------------------------------------------ */

static q15_t q15_saturate(int32_t v)
{
    return (v > INT16_MAX) ? INT16_MAX : ((v < INT16_MIN) ? INT16_MIN : (q15_t)v);
}

q15_t q15_mul(q15_t a, q15_t b)
{
    /* -1 * -1 would overflow to +1: saturation handles it */
    return q15_saturate((((int32_t)a * b) + ((int32_t)1 << 14)) >> 15);
}

q15_t q15_add_sat(q15_t a, q15_t b)
{
    return q15_saturate((int32_t)a + b);
}

q15_t q15_sub_sat(q15_t a, q15_t b)
{
    return q15_saturate((int32_t)a - b);
}

/* ------------------------- table sine ----------------------------- */

#define SIN_TABLE_BITS  (6U)                           /* 64 segments */
#define SIN_TABLE_SIZE  ((1U << SIN_TABLE_BITS) + 1U)  /* + end point */

static const int16_t sin_quarter_q15[SIN_TABLE_SIZE] = {
        0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
     6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767
};

q15_t fx_sin_q15(uint16_t angle)
{
    /* bits 15..14: quadrant, bits 13..8: table index, bits 7..0: frac */
    const uint32_t quadrant = (uint32_t)angle >> 14;
    uint32_t       pos      = (uint32_t)angle & 0x3FFFU;   /* within quadrant */

    if ((quadrant & 1U) != 0U) {
        pos = 0x4000U - pos;           /* mirror: 2nd and 4th quadrants */
    }
    const uint32_t index = pos >> 8;
    const uint32_t frac  = pos & 0xFFU;
    int32_t        value = sin_quarter_q15[index];
    if (index < (SIN_TABLE_SIZE - 1U)) {
        value += (((int32_t)sin_quarter_q15[index + 1U] - value) * (int32_t)frac) >> 8;
    }
    return (q15_t)((quadrant >= 2U) ? -value : value);
}

q15_t fx_cos_q15(uint16_t angle)
{
    return fx_sin_q15((uint16_t)(angle + 0x4000U));
}

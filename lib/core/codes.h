/**
 * @file    codes.h
 * @brief   Number codes and line codes: BCD, Gray, 7-segment, Morse,
 *          Manchester, Hamming(7,4), ZigZag, Base64.
 *
 * Pure functions, table-driven where it makes sense, portable.
 */
#ifndef CODES_H
#define CODES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------ BCD ------------------------------- */
/** 1234 -> 0x1234 (max 99999999). Double-dabble free: divide by 10. */
uint32_t codes_bin_to_bcd(uint32_t value);
/** 0x1234 -> 1234. Returns false if a nibble is > 9. */
bool     codes_bcd_to_bin(uint32_t bcd, uint32_t *value);

/* ------------------------------ Gray ------------------------------ */
/** Reflected binary Gray code: g = b ^ (b >> 1). */
static inline uint32_t codes_bin_to_gray(uint32_t b) { return b ^ (b >> 1); }
uint32_t codes_gray_to_bin(uint32_t g);

/* --------------------------- 7-segment ---------------------------- */
/*
 *      --a--
 *     |     |      bit0 = a ... bit6 = g, bit7 = dp
 *     f     b      active-high (invert for common-anode displays)
 *     |--g--|
 *     e     c
 *     |     |
 *      --d--  .dp
 */
#define SEG7_A   (1U << 0)
#define SEG7_B   (1U << 1)
#define SEG7_C   (1U << 2)
#define SEG7_D   (1U << 3)
#define SEG7_E   (1U << 4)
#define SEG7_F   (1U << 5)
#define SEG7_G   (1U << 6)
#define SEG7_DP  (1U << 7)

/** Segments for a hex digit 0..F. */
uint8_t codes_seg7_hex(uint8_t nibble);
/** Segments for an ASCII char (digits, most letters, '-', '_', ' '). */
uint8_t codes_seg7_char(char c);

/* ------------------------------ Morse ----------------------------- */
/**
 * @brief  Morse pattern of a char.
 * @param  pattern out: bits LSB-first, 1 = dash, 0 = dot
 * @return number of symbols (0 if the char has no Morse code)
 */
uint8_t codes_morse(char c, uint8_t *pattern);

/* --------------------------- Manchester --------------------------- */
/** IEEE 802.3 convention: 0 -> 10, 1 -> 01. One byte -> 16 bit. */
uint16_t codes_manchester_encode(uint8_t byte);
/** Returns false on an invalid symbol pair (00 or 11). */
bool     codes_manchester_decode(uint16_t word, uint8_t *byte);

/* ---------------------------- Hamming ----------------------------- */
/** Encode 4 data bits into a 7-bit Hamming code (p1 p2 d1 p3 d2 d3 d4). */
uint8_t codes_hamming74_encode(uint8_t nibble);
/**
 * @brief  Decode and correct a single-bit error.
 * @return position of the corrected bit (1..7), 0 if no error.
 */
uint8_t codes_hamming74_decode(uint8_t code, uint8_t *nibble);

/* ----------------------------- ZigZag ----------------------------- */
/** Map signed to unsigned so small magnitudes stay small (protobuf). */
static inline uint32_t codes_zigzag_encode(int32_t v)
{
    return ((uint32_t)v << 1) ^ (uint32_t)(v >> 31);
}
static inline int32_t codes_zigzag_decode(uint32_t u)
{
    return (int32_t)(u >> 1) ^ -(int32_t)(u & 1U);
}

/* ----------------------------- Base64 ----------------------------- */
/** Returns the output length (NUL not counted), 0 if out is too small. */
size_t codes_base64_encode(const uint8_t *in, size_t len, char *out, size_t out_size);
/** Returns the decoded length, 0 on error. */
size_t codes_base64_decode(const char *in, uint8_t *out, size_t out_size);

#endif /* CODES_H */

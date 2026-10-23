/**
 * @file    codes.c
 * @brief   Number and line codes (see codes.h).
 */
#include "codes.h"
#include "strutil.h"

/* ------------------------------ BCD ------------------------------- */

uint32_t codes_bin_to_bcd(uint32_t value)
{
    uint32_t bcd   = 0U;
    uint32_t shift = 0U;

    if (value > 99999999UL) {
        value = 99999999UL;
    }
    do {
        bcd   |= (value % 10U) << shift;
        value /= 10U;
        shift += 4U;
    } while (value != 0U);
    return bcd;
}

bool codes_bcd_to_bin(uint32_t bcd, uint32_t *value)
{
    uint32_t result = 0U;

    for (int32_t shift = 28; shift >= 0; shift -= 4) {
        const uint32_t digit = (bcd >> (uint32_t)shift) & 0xFU;
        if (digit > 9U) {
            return false;
        }
        result = (result * 10U) + digit;
    }
    *value = result;
    return true;
}

/* ------------------------------ Gray ------------------------------ */

uint32_t codes_gray_to_bin(uint32_t g)
{
    /* prefix XOR in log2(32) = 5 steps instead of a 32-step loop */
    g ^= g >> 16;
    g ^= g >> 8;
    g ^= g >> 4;
    g ^= g >> 2;
    g ^= g >> 1;
    return g;
}

/* --------------------------- 7-segment ---------------------------- */

static const uint8_t seg7_hex_table[16] = {
    0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U, 0x6DU, 0x7DU, 0x07U,  /* 0-7 */
    0x7FU, 0x6FU, 0x77U, 0x7CU, 0x39U, 0x5EU, 0x79U, 0x71U   /* 8-F */
};

/* Best-effort letters; some (K, M, W, X) are approximations. */
static const uint8_t seg7_alpha_table[26] = {
    0x77U, 0x7CU, 0x39U, 0x5EU, 0x79U, 0x71U, 0x3DU, 0x76U,  /* A-H */
    0x30U, 0x1EU, 0x75U, 0x38U, 0x37U, 0x54U, 0x5CU, 0x73U,  /* I-P */
    0x67U, 0x50U, 0x6DU, 0x78U, 0x3EU, 0x1CU, 0x2AU, 0x76U,  /* Q-X */
    0x6EU, 0x5BU                                             /* Y-Z */
};

uint8_t codes_seg7_hex(uint8_t nibble)
{
    return seg7_hex_table[nibble & 0x0FU];
}

uint8_t codes_seg7_char(char c)
{
    if (chr_is_digit(c)) {
        return seg7_hex_table[c - '0'];
    }
    if (chr_is_alpha(c)) {
        return seg7_alpha_table[chr_to_upper(c) - 'A'];
    }
    switch (c) {
    case '-':  return SEG7_G;
    case '_':  return SEG7_D;
    case '=':  return (uint8_t)(SEG7_G | SEG7_D);
    case '.':  return SEG7_DP;
    case '\'': return SEG7_B;
    default:   return 0U;
    }
}

/* ------------------------------ Morse ----------------------------- */

static const char *const morse_alpha[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",  "....",
    "..",   ".---", "-.-",  ".-..", "--",   "-.",   "---",  ".--.",
    "--.-", ".-.",  "...",  "-",    "..-",  "...-", ".--",  "-..-",
    "-.--", "--.."
};

static const char *const morse_digit[10] = {
    "-----", ".----", "..---", "...--", "....-",
    ".....", "-....", "--...", "---..", "----."
};

uint8_t codes_morse(char c, uint8_t *pattern)
{
    const char *code;
    uint8_t     len  = 0U;
    uint8_t     bits = 0U;

    if (chr_is_alpha(c)) {
        code = morse_alpha[chr_to_upper(c) - 'A'];
    } else if (chr_is_digit(c)) {
        code = morse_digit[c - '0'];
    } else {
        *pattern = 0U;
        return 0U;
    }
    for (; code[len] != '\0'; len++) {
        if (code[len] == '-') {
            bits |= (uint8_t)(1U << len);
        }
    }
    *pattern = bits;
    return len;
}

/* --------------------------- Manchester --------------------------- */

uint16_t codes_manchester_encode(uint8_t byte)
{
    uint16_t out = 0U;
    for (uint32_t i = 0U; i < 8U; i++) {
        /* bit 1 -> 01, bit 0 -> 10 (two output bits per input bit) */
        const uint16_t sym = ((byte >> i) & 1U) ? 0x1U : 0x2U;
        out |= (uint16_t)(sym << (2U * i));
    }
    return out;
}

bool codes_manchester_decode(uint16_t word, uint8_t *byte)
{
    uint8_t out = 0U;
    for (uint32_t i = 0U; i < 8U; i++) {
        const uint32_t sym = (word >> (2U * i)) & 0x3U;
        if (sym == 0x1U) {
            out |= (uint8_t)(1U << i);
        } else if (sym != 0x2U) {
            return false;
        } else {
            /* bit is 0 */
        }
    }
    *byte = out;
    return true;
}

/* ---------------------------- Hamming ----------------------------- */
/* Code bit k (0-based) is Hamming position k+1:
 * pos: 1  2  3  4  5  6  7
 *      p1 p2 d1 p3 d2 d3 d4                                          */

#define HAM_BIT(code, pos)  (((code) >> ((pos) - 1U)) & 1U)

uint8_t codes_hamming74_encode(uint8_t nibble)
{
    const uint8_t d1 = (nibble >> 0) & 1U;
    const uint8_t d2 = (nibble >> 1) & 1U;
    const uint8_t d3 = (nibble >> 2) & 1U;
    const uint8_t d4 = (nibble >> 3) & 1U;
    const uint8_t p1 = d1 ^ d2 ^ d4;
    const uint8_t p2 = d1 ^ d3 ^ d4;
    const uint8_t p3 = d2 ^ d3 ^ d4;

    return (uint8_t)((p1 << 0) | (p2 << 1) | (d1 << 2) | (p3 << 3) |
                     (d2 << 4) | (d3 << 5) | (d4 << 6));
}

uint8_t codes_hamming74_decode(uint8_t code, uint8_t *nibble)
{
    /* each syndrome bit re-checks the positions whose index has it set */
    const uint8_t s1 = HAM_BIT(code, 1U) ^ HAM_BIT(code, 3U) ^ HAM_BIT(code, 5U) ^ HAM_BIT(code, 7U);
    const uint8_t s2 = HAM_BIT(code, 2U) ^ HAM_BIT(code, 3U) ^ HAM_BIT(code, 6U) ^ HAM_BIT(code, 7U);
    const uint8_t s3 = HAM_BIT(code, 4U) ^ HAM_BIT(code, 5U) ^ HAM_BIT(code, 6U) ^ HAM_BIT(code, 7U);
    const uint8_t syndrome = (uint8_t)(s1 | (s2 << 1) | (s3 << 2));

    if (syndrome != 0U) {
        code ^= (uint8_t)(1U << (syndrome - 1U));   /* flip the bad bit */
    }
    *nibble = (uint8_t)(HAM_BIT(code, 3U) | (HAM_BIT(code, 5U) << 1) |
                        (HAM_BIT(code, 6U) << 2) | (HAM_BIT(code, 7U) << 3));
    return syndrome;
}

/* ----------------------------- Base64 ----------------------------- */

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t codes_base64_encode(const uint8_t *in, size_t len, char *out, size_t out_size)
{
    const size_t needed = (((len + 2U) / 3U) * 4U);
    size_t       o      = 0U;

    if ((needed + 1U) > out_size) {
        return 0U;
    }
    for (size_t i = 0U; i < len; i += 3U) {
        const uint32_t remaining = (uint32_t)(len - i);
        uint32_t       triple    = (uint32_t)in[i] << 16;
        if (remaining > 1U) { triple |= (uint32_t)in[i + 1U] << 8; }
        if (remaining > 2U) { triple |= (uint32_t)in[i + 2U]; }

        out[o++] = b64_table[(triple >> 18) & 0x3FU];
        out[o++] = b64_table[(triple >> 12) & 0x3FU];
        out[o++] = (remaining > 1U) ? b64_table[(triple >> 6) & 0x3FU] : '=';
        out[o++] = (remaining > 2U) ? b64_table[triple & 0x3FU] : '=';
    }
    out[o] = '\0';
    return o;
}

static int b64_value(char c)
{
    if (chr_is_upper(c)) { return c - 'A'; }
    if (chr_is_lower(c)) { return (c - 'a') + 26; }
    if (chr_is_digit(c)) { return (c - '0') + 52; }
    if (c == '+')        { return 62; }
    if (c == '/')        { return 63; }
    return -1;
}

size_t codes_base64_decode(const char *in, uint8_t *out, size_t out_size)
{
    uint32_t acc   = 0U;
    uint32_t nbits = 0U;
    size_t   o     = 0U;

    for (; (*in != '\0') && (*in != '='); in++) {
        const int v = b64_value(*in);
        if (v < 0) {
            return 0U;
        }
        acc    = (acc << 6) | (uint32_t)v;
        nbits += 6U;
        if (nbits >= 8U) {
            nbits -= 8U;
            if (o >= out_size) {
                return 0U;
            }
            out[o++] = (uint8_t)(acc >> nbits);
        }
    }
    return o;
}

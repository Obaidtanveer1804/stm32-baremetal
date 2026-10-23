/**
 * @file    test_core.c
 * @brief   Unit tests for the portable core modules (run on the PC).
 */
#include "unit.h"

#include "bits.h"
#include "codes.h"
#include "counter.h"
#include "crc.h"
#include "debounce.h"
#include "evq.h"
#include "filter.h"
#include "fixmath.h"
#include "fmt.h"
#include "framing.h"
#include "fsm.h"
#include "scpi.h"
#include "dsp.h"
#include "bitset.h"
#include "dds.h"
#include "list.h"
#include "menu.h"
#include "modbus.h"
#include "pool.h"
#include "sort.h"
#include "trace.h"
#include "geo.h"
#include "gfx.h"
#include "nmea.h"
#include "io.h"
#include "pid.h"
#include "rand.h"
#include "ringbuf.h"
#include "softtimer.h"
#include "stats.h"
#include "strutil.h"

int unit_checks;
int unit_failures;

/* io capture: everything printed through io_* lands in this buffer */
static char   s_out[2048];
static size_t s_out_len;

static void capture_putc(char ch)
{
    if (s_out_len < (sizeof(s_out) - 1U)) {
        s_out[s_out_len++] = ch;
        s_out[s_out_len]   = '\0';
    }
}

static void capture_reset(void)
{
    s_out_len = 0U;
    s_out[0]  = '\0';
}

/* ------------------------------------------------------------------ */

TEST(bits)
{
    uint32_t reg = 0U;
    CHECK_EQ_INT(BIT(5), 32);
    CHECK_EQ_INT(BIT_MASK(4), 0xF);
    CHECK_EQ_INT(BIT_MASK(32), 0xFFFFFFFFU);
    BIT_SET(reg, BIT(3) | BIT(0));
    CHECK_EQ_INT(reg, 9);
    BIT_CLR(reg, BIT(0));
    CHECK_EQ_INT(reg, 8);
    FIELD_SET(reg, 4, 3, 5);
    CHECK_EQ_INT(FIELD_GET(reg, 4, 3), 5);
    CHECK_EQ_INT(reg, 0x58);
    CHECK_EQ_INT(bits_popcount(0xF0F0F0F0U), 16);
    CHECK_EQ_INT(bits_parity(7U), 1);
    CHECK_EQ_INT(bits_parity(3U), 0);
    CHECK_EQ_INT(bits_reverse8(0x01U), 0x80);
    CHECK_EQ_INT(bits_reverse8(0xA0U), 0x05);
    CHECK_EQ_INT(bits_reverse32(1U), 0x80000000U);
    CHECK_EQ_INT(bits_clz(1U), 31);
    CHECK_EQ_INT(bits_clz(0U), 32);
    CHECK_EQ_INT(bits_ctz(8U), 3);
    CHECK_EQ_INT(bits_msb_index(1000U), 9);
    CHECK_EQ_INT(bits_next_pow2(100U), 128);
    CHECK_EQ_INT(bits_next_pow2(64U), 64);
    CHECK(IS_POW2(64U));
    CHECK(!IS_POW2(65U));
    CHECK_EQ_INT(ALIGN_UP(13U, 8U), 16);
    CHECK_EQ_INT(bits_rotl32(0x80000001U, 1U), 3);
    CHECK_EQ_INT(bits_bswap32(0x11223344U), 0x44332211U);
    CHECK_EQ_INT(bits_sign_extend(0xFFFU, 12U), -1);
    CHECK_EQ_INT(bits_sign_extend(0x7FFU, 12U), 2047);
    CHECK_EQ_INT(bits_abs32(-42), 42);
    CHECK(bits_opposite_sign(-1, 1));
    CHECK_EQ_INT(bits_morton2(3U, 0U), 5);
}

TEST(ringbuf)
{
    uint8_t   storage[8];
    ringbuf_t rb;
    uint8_t   b = 0U;

    CHECK(!ringbuf_init(&rb, storage, 6U));
    CHECK(ringbuf_init(&rb, storage, 8U));
    CHECK(ringbuf_is_empty(&rb));
    for (uint8_t i = 0U; i < 8U; i++) {
        CHECK(ringbuf_put(&rb, i));
    }
    CHECK(ringbuf_is_full(&rb));
    CHECK(!ringbuf_put(&rb, 99U));
    CHECK(ringbuf_get(&rb, &b));
    CHECK_EQ_INT(b, 0);
    CHECK(ringbuf_peek_at(&rb, 2U, &b));
    CHECK_EQ_INT(b, 3);
    /* wrap the free-running indices many times */
    for (uint32_t i = 0U; i < 70000U; i++) {
        (void)ringbuf_get(&rb, &b);
        (void)ringbuf_put(&rb, (uint8_t)i);
    }
    CHECK_EQ_INT(ringbuf_count(&rb), 7);
    ringbuf_put_overwrite(&rb, 0xAAU);
    ringbuf_put_overwrite(&rb, 0xBBU);
    CHECK_EQ_INT(ringbuf_count(&rb), 8);
    CHECK_EQ_INT(ringbuf_skip(&rb, 100U), 8);
    CHECK(ringbuf_is_empty(&rb));
}

TEST(fmt)
{
    char buf[64];

    fmt_snprintf(buf, sizeof(buf), "%d|%5d|%-5d|%05d", -12, 34, 56, -7);
    CHECK_EQ_STR(buf, "-12|   34|56   |-0007");
    fmt_snprintf(buf, sizeof(buf), "%x %X %#x %08b %o", 255U, 255U, 255U, 5U, 8U);
    CHECK_EQ_STR(buf, "ff FF 0xff 00000101 10");
    fmt_snprintf(buf, sizeof(buf), "%s|%.3s|%6s|%-6s|%c", "abc", "abcdef", "ab", "ab", 'z');
    CHECK_EQ_STR(buf, "abc|abc|    ab|ab    |z");
    fmt_snprintf(buf, sizeof(buf), "%ld %lu %lld %u%%", -2147483647L - 1L, 4294967295UL,
                 -9000000000LL, 7U);
    CHECK_EQ_STR(buf, "-2147483648 4294967295 -9000000000 7%");
    fmt_snprintf(buf, sizeof(buf), "%+d % d %.4d %*d", 5, 5, 42, 4, 1);
    CHECK_EQ_STR(buf, "+5  5 0042    1");
    CHECK_EQ_INT(fmt_snprintf(buf, 4U, "hello"), 5);
    CHECK_EQ_STR(buf, "hel");
    fmt_u32(buf, sizeof(buf), 0xBEEFU, 16U, 6U);
    CHECK_EQ_STR(buf, "00BEEF");
    fmt_bin(buf, sizeof(buf), 0xA5U, 8U, true);
    CHECK_EQ_STR(buf, "1010_0101");
    fmt_fixed(buf, sizeof(buf), 12345, 100U, 2U);
    CHECK_EQ_STR(buf, "123.45");
    fmt_fixed(buf, sizeof(buf), -5, 100U, 2U);
    CHECK_EQ_STR(buf, "-0.05");
    fmt_fixed(buf, sizeof(buf), 1999, 1000U, 2U);
    CHECK_EQ_STR(buf, "2.00");
    fmt_i32(buf, sizeof(buf), -2147483647 - 1);
    CHECK_EQ_STR(buf, "-2147483648");
}

TEST(strutil)
{
    char     s[64];
    char    *argv[8];
    uint32_t u = 0U;
    int32_t  i = 0;
    bool     b = false;
    uint8_t  bytes[8];

    str_copy(s, "  hello world  ", sizeof(s));
    CHECK_EQ_STR(str_trim(s), "hello world");
    CHECK(str_ieq("GPIO", "gpio"));
    CHECK(str_starts_with("gpio set", "gpio"));
    CHECK(str_ends_with("file.c", ".c"));
    CHECK_EQ_INT(str_copy(s, "abcdef", 4U), 6);
    CHECK_EQ_STR(s, "abc");
    str_copy(s, "ab", sizeof(s));
    str_cat(s, "cd", sizeof(s));
    CHECK_EQ_STR(s, "abcd");
    CHECK_EQ_STR(str_reverse(s), "dcba");
    str_copy(s, "led  set \"two words\" 3", sizeof(s));
    CHECK_EQ_INT(str_tokenize(s, argv, 8U), 4);
    CHECK_EQ_STR(argv[0], "led");
    CHECK_EQ_STR(argv[2], "two words");
    CHECK_EQ_STR(argv[3], "3");
    str_copy(s, "a,b,,c", sizeof(s));
    CHECK_EQ_INT(str_split(s, ',', argv, 8U), 4);
    CHECK_EQ_STR(argv[2], "");
    CHECK(str_to_u32("0x1F", &u) && (u == 31U));
    CHECK(str_to_u32("0b1010_0101", &u) && (u == 0xA5U));
    CHECK(str_to_u32("4294967295", &u) && (u == 4294967295U));
    CHECK(!str_to_u32("4294967296", &u));
    CHECK(!str_to_u32("12a", &u));
    CHECK(!str_to_u32("", &u));
    CHECK(str_to_i32("-2147483648", &i) && (i == INT32_MIN));
    CHECK(!str_to_i32("2147483648", &i));
    CHECK(str_to_fixed("-1.25", 100U, &i) && (i == -125));
    CHECK(str_to_fixed("3.14159", 1000U, &i) && (i == 3142));
    CHECK(str_to_bool("ON", &b) && b);
    CHECK(str_to_bool("low", &b) && !b);
    CHECK_EQ_INT(str_hex_to_bytes("de:ad be EF", bytes, sizeof(bytes)), 4);
    CHECK_EQ_INT(bytes[3], 0xEF);
    CHECK(str_str("hello", "ll") != NULL);
    CHECK(str_rchr("a.b.c", '.') != NULL);
}

TEST(codes)
{
    uint32_t v       = 0U;
    uint8_t  pattern = 0U;
    uint8_t  nib     = 0U;
    char     b64[32];
    uint8_t  raw[16];

    CHECK_EQ_INT(codes_bin_to_bcd(1234U), 0x1234);
    CHECK(codes_bcd_to_bin(0x9876U, &v) && (v == 9876U));
    CHECK(!codes_bcd_to_bin(0x1AU, &v));
    for (uint32_t x = 0U; x < 1000U; x++) {
        CHECK_EQ_INT(codes_gray_to_bin(codes_bin_to_gray(x)), x);
        /* consecutive Gray codes differ by exactly one bit */
        CHECK_EQ_INT(bits_popcount(codes_bin_to_gray(x) ^ codes_bin_to_gray(x + 1U)), 1);
    }
    CHECK_EQ_INT(codes_seg7_hex(8U), 0x7F);
    CHECK_EQ_INT(codes_seg7_char('-'), SEG7_G);
    CHECK_EQ_INT(codes_morse('S', &pattern), 3);
    CHECK_EQ_INT(pattern, 0);
    CHECK_EQ_INT(codes_morse('o', &pattern), 3);
    CHECK_EQ_INT(pattern, 7);
    CHECK_EQ_INT(codes_morse('A', &pattern), 2);
    CHECK_EQ_INT(pattern, 2);   /* .- : dot then dash -> bit1 set */
    for (uint32_t x = 0U; x < 256U; x++) {
        uint8_t back = 0U;
        CHECK(codes_manchester_decode(codes_manchester_encode((uint8_t)x), &back));
        CHECK_EQ_INT(back, x);
    }
    CHECK(!codes_manchester_decode(0x0003U, &nib));
    for (uint8_t n = 0U; n < 16U; n++) {
        const uint8_t code = codes_hamming74_encode(n);
        CHECK_EQ_INT(codes_hamming74_decode(code, &nib), 0);
        CHECK_EQ_INT(nib, n);
        for (uint8_t bit = 0U; bit < 7U; bit++) {
            CHECK_EQ_INT(codes_hamming74_decode((uint8_t)(code ^ (1U << bit)), &nib), bit + 1U);
            CHECK_EQ_INT(nib, n);
        }
    }
    CHECK_EQ_INT(codes_zigzag_encode(-1), 1);
    CHECK_EQ_INT(codes_zigzag_encode(1), 2);
    CHECK_EQ_INT(codes_zigzag_decode(3U), -2);
    codes_base64_encode((const uint8_t *)"Man", 3U, b64, sizeof(b64));
    CHECK_EQ_STR(b64, "TWFu");
    codes_base64_encode((const uint8_t *)"Ma", 2U, b64, sizeof(b64));
    CHECK_EQ_STR(b64, "TWE=");
    CHECK_EQ_INT(codes_base64_decode("aGVsbG8=", raw, sizeof(raw)), 5);
    CHECK(memcmp(raw, "hello", 5U) == 0);
}

TEST(crc)
{
    const uint8_t *check = (const uint8_t *)"123456789";
    CHECK_EQ_INT(crc8_maxim(CRC8_MAXIM_INIT, check, 9U), 0xA1);
    CHECK_EQ_INT(crc8_smbus(CRC8_SMBUS_INIT, check, 9U), 0xF4);
    CHECK_EQ_INT(crc16_ccitt(CRC16_CCITT_INIT, check, 9U), 0x29B1);
    CHECK_EQ_INT(crc16_modbus(CRC16_MODBUS_INIT, check, 9U), 0x4B37);
    CHECK_EQ_INT(crc16_xmodem(CRC16_XMODEM_INIT, check, 9U), 0x31C3);
    CHECK_EQ_INT(crc32(check, 9U), 0xCBF43926U);
    /* incremental = one shot */
    uint32_t c = crc32_update(CRC32_INIT, check, 4U);
    c = crc32_update(c, &check[4], 5U);
    CHECK_EQ_INT(crc32_final(c), 0xCBF43926U);
    CHECK_EQ_INT(checksum_fletcher16((const uint8_t *)"abcde", 5U), 0xC8F0);
    CHECK_EQ_INT(checksum_adler32((const uint8_t *)"Wikipedia", 9U), 0x11E60398U);
    CHECK_EQ_INT(checksum_nmea("$GPGLL,5300.97914,N,00259.98174,E,125926,A*28"), 0x28);
    const uint8_t hex_rec[] = { 0x03U, 0x00U, 0x30U, 0x00U, 0x02U, 0x33U, 0x7AU };
    CHECK_EQ_INT(checksum_twos8(hex_rec, sizeof(hex_rec)), 0x1E);
}

TEST(fixmath)
{
    CHECK_EQ_INT(fx_isqrt32(0U), 0);
    CHECK_EQ_INT(fx_isqrt32(99U), 9);
    CHECK_EQ_INT(fx_isqrt32(4294967295U), 65535);
    CHECK_EQ_INT(fx_isqrt64(1ULL << 62), 1U << 31);
    CHECK_EQ_INT(fx_ilog2(1024U), 10);
    CHECK_EQ_INT(fx_ipow(3U, 4U), 81);
    CHECK_EQ_INT(fx_ipow(2U, 40U), 0xFFFFFFFFU);
    CHECK_EQ_INT(fx_gcd(48U, 18U), 6);
    CHECK_EQ_INT(fx_lcm(4U, 6U), 12);
    CHECK_EQ_INT(fx_div_round(7, 2), 4);
    CHECK_EQ_INT(fx_div_round(-7, 2), -4);
    CHECK_EQ_INT(fx_map(512, 0, 1023, 0, 100), 50);
    CHECK_EQ_INT(fx_add_sat(INT32_MAX, 1), INT32_MAX);
    CHECK_EQ_INT(fx_uadd_sat(0xFFFFFFF0U, 0x20U), 0xFFFFFFFFU);
    for (uint32_t x = 0U; x < 100000U; x += 7U) {
        CHECK_EQ_INT(fx_div10(x), x / 10U);
    }
    CHECK_EQ_INT(fx_div10(4294967295U), 429496729U);

    CHECK_EQ_INT(q16_mul(Q16_CONST(1.5), Q16_CONST(2.0)), Q16_CONST(3.0));
    CHECK_EQ_INT(q16_div(Q16_FROM_INT(1), Q16_FROM_INT(4)), Q16_CONST(0.25));
    CHECK_EQ_INT(q16_div(Q16_FROM_INT(1), 0), Q16_MAX);
    CHECK_EQ_INT(q16_sqrt(Q16_FROM_INT(2)), 92682);     /* 1.41421, rounded */
    CHECK_EQ_INT(q16_sqrt(Q16_FROM_INT(9)), Q16_FROM_INT(3));
    CHECK_EQ_INT(q16_from_ratio(1, 3), 21845);
    CHECK_EQ_INT(q16_round(Q16_CONST(2.5)), 3);
    CHECK_NEAR_INT(q16_sin(BRAD_FROM_DEG(30)), Q16_CONST(0.5), 3);
    CHECK_NEAR_INT(q16_sin(BRAD_FROM_DEG(90)), Q16_ONE, 3);
    CHECK_NEAR_INT(q16_cos(BRAD_FROM_DEG(180)), -Q16_ONE, 3);
    CHECK_NEAR_INT(q16_sin(BRAD_FROM_DEG(270)), -Q16_ONE, 3);
    CHECK_NEAR_INT(q16_cos(BRAD_FROM_DEG(60)), Q16_CONST(0.5), 6);  /* angle truncated to 10922 brad */
    CHECK_NEAR_INT(q16_sin(BRAD_FROM_DEG(-45)), Q16_CONST(-0.70711), 3);
    CHECK_NEAR_INT(q16_atan2(1, 1), 8192, 2);           /* 45 deg   */
    CHECK_NEAR_INT(q16_atan2(1, -1), 24576, 2);         /* 135 deg  */
    CHECK_NEAR_INT(q16_atan2(-1, -1), 40960, 2);        /* 225 deg  */
    CHECK_NEAR_INT(q16_atan2(-1000, 0), 49152, 2);      /* 270 deg  */
    CHECK_NEAR_INT(q16_atan2(0, -5), 32768, 2);         /* 180 deg  */
    CHECK_EQ_INT(fx_hypot(3, 4), 5);
    CHECK_NEAR_INT(fx_sin_q15(BRAD_FROM_DEG(30)), 16384, 40);
    CHECK_NEAR_INT(fx_sin_q15(BRAD_FROM_DEG(210)), -16384, 40);
    CHECK_NEAR_INT(fx_cos_q15(0U), 32767, 2);
    CHECK_EQ_INT(q15_mul(Q15_CONST(0.5), Q15_CONST(0.5)), Q15_CONST(0.25));
    CHECK_EQ_INT(q15_mul(-32768, -32768), 32767);
    CHECK_EQ_INT(q15_add_sat(30000, 10000), 32767);
}

TEST(filter)
{
    int32_t     window[4];
    filt_mavg_t ma;
    filt_mavg_init(&ma, window, 4U);
    CHECK_EQ_INT(filt_mavg_update(&ma, 4), 4);
    CHECK_EQ_INT(filt_mavg_update(&ma, 8), 6);
    (void)filt_mavg_update(&ma, 8);
    (void)filt_mavg_update(&ma, 8);
    CHECK_EQ_INT(filt_mavg_update(&ma, 8), 8);

    filt_ema_t ema;
    filt_ema_init(&ema, 2U);
    CHECK_EQ_INT(filt_ema_update(&ema, 100), 100);
    for (int k = 0; k < 60; k++) {
        (void)filt_ema_update(&ema, 0);
    }
    CHECK_EQ_INT(filt_ema_update(&ema, 0), 0);

    CHECK_EQ_INT(filt_median3(3, 1, 2), 2);
    CHECK_EQ_INT(filt_median3(1, 3, 2), 2);
    CHECK_EQ_INT(filt_median3(2, 2, 9), 2);
    filt_median_t med;
    filt_median_init(&med, 5U);
    (void)filt_median_update(&med, 10);
    (void)filt_median_update(&med, 10);
    (void)filt_median_update(&med, 1000);    /* spike */
    (void)filt_median_update(&med, 10);
    CHECK_EQ_INT(filt_median_update(&med, 12), 10);

    filt_hyst_t h;
    filt_hyst_init(&h, 10, 20, false);
    CHECK(!filt_hyst_update(&h, 15));
    CHECK(filt_hyst_update(&h, 21));
    CHECK(filt_hyst_update(&h, 15));
    CHECK(!filt_hyst_update(&h, 9));

    filt_slew_t s;
    filt_slew_init(&s, 0, 10);
    CHECK_EQ_INT(filt_slew_update(&s, 100), 10);
    CHECK_EQ_INT(filt_slew_update(&s, 15), 15);
    CHECK_EQ_INT(filt_deadband(102, 100, 3), 100);

    /* biquad as a pure gain of 1 (b0 = 1.0) */
    filt_biquad_t bq;
    filt_biquad_init(&bq, 16384, 0, 0, 0, 0);
    CHECK_EQ_INT(filt_biquad_update(&bq, 1234), 1234);

    /* FIR: 2-tap average, 0.5 = 16384 in Q15 */
    static const int16_t taps[2] = { 16384, 16384 };
    int32_t    hist[2];
    filt_fir_t fir;
    filt_fir_init(&fir, taps, hist, 2U);
    CHECK_EQ_INT(filt_fir_update(&fir, 100), 50);
    CHECK_EQ_INT(filt_fir_update(&fir, 200), 150);

    const int32_t samples[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2 };
    CHECK_EQ_INT(filt_oversample(samples, 2U), 6);   /* 24 >> 2 */
}

TEST(pid)
{
    pid_ctrl_t pid;
    /* P only, gain 2 */
    pid_init(&pid, Q16_FROM_INT(2), 0, 0, -1000, 1000);
    CHECK_EQ_INT(pid_update(&pid, 100, 90), 20);
    CHECK_EQ_INT(pid_update(&pid, 100, 0), 200);
    CHECK_EQ_INT(pid_update(&pid, 1000, 0), 1000);   /* clamped */

    /* I only: integrates 0.5 * error per step */
    pid_init(&pid, 0, Q16_CONST(0.5), 0, -100, 100);
    CHECK_EQ_INT(pid_update(&pid, 10, 0), 5);
    CHECK_EQ_INT(pid_update(&pid, 10, 0), 10);
    for (int k = 0; k < 100; k++) {
        (void)pid_update(&pid, 10, 0);
    }
    CHECK_EQ_INT(pid_update(&pid, 10, 0), 100);      /* saturated, no windup */
    CHECK(pid_update(&pid, 0, 10) < 100);            /* reacts immediately */

    /* closed loop on a simple first-order plant reaches the setpoint */
    int32_t y = 0;
    pid_init(&pid, Q16_CONST(0.5), Q16_CONST(0.1), 0, -1000, 1000);
    for (int k = 0; k < 400; k++) {
        const int32_t u = pid_update(&pid, 500, y);
        y += (u - y) / 4;
    }
    CHECK_NEAR_INT(y, 500, 5);
}

TEST(debounce)
{
    debounce_vc_t d;
    debounce_vc_init(&d, 0U);
    CHECK_EQ_INT(debounce_vc_update(&d, 1U), 0);
    CHECK_EQ_INT(debounce_vc_update(&d, 0U), 0);     /* bounce resets */
    CHECK_EQ_INT(debounce_vc_update(&d, 1U), 0);
    CHECK_EQ_INT(debounce_vc_update(&d, 1U), 0);
    CHECK_EQ_INT(debounce_vc_update(&d, 1U), 0);
    CHECK_EQ_INT(debounce_vc_update(&d, 1U), 1);     /* 4th stable sample */
    CHECK_EQ_INT(d.state, 1);
    CHECK_EQ_INT(debounce_vc_take_pressed(&d, 0xFFFFFFFFU), 1);
    CHECK_EQ_INT(debounce_vc_take_pressed(&d, 0xFFFFFFFFU), 0);

    btn_t       b;
    btn_event_t ev;
    uint32_t    t = 0U;
    uint32_t    counts[7] = { 0 };
    btn_init(&b, NULL);
    /* single click: press 100 ms, then wait */
    for (; t < 100U; t++) { counts[btn_update(&b, true, t)]++; }
    for (; t < 600U; t++) { counts[btn_update(&b, false, t)]++; }
    CHECK_EQ_INT(counts[BTN_EVENT_PRESS], 1);
    CHECK_EQ_INT(counts[BTN_EVENT_CLICK], 1);
    /* double click */
    for (uint32_t k = 0U; k < 7U; k++) { counts[k] = 0U; }
    for (uint32_t r = 0U; r < 2U; r++) {
        const uint32_t t0 = t;
        for (; t < (t0 + 80U); t++)  { counts[btn_update(&b, true, t)]++; }
        for (; t < (t0 + 160U); t++) { counts[btn_update(&b, false, t)]++; }
    }
    for (const uint32_t t0 = t; t < (t0 + 500U); t++) { counts[btn_update(&b, false, t)]++; }
    CHECK_EQ_INT(counts[BTN_EVENT_DOUBLE_CLICK], 1);
    CHECK_EQ_INT(counts[BTN_EVENT_CLICK], 0);
    /* long press with repeats */
    for (uint32_t k = 0U; k < 7U; k++) { counts[k] = 0U; }
    for (const uint32_t t0 = t; t < (t0 + 1200U); t++) {
        ev = btn_update(&b, true, t);
        counts[ev]++;
    }
    CHECK_EQ_INT(counts[BTN_EVENT_LONG_PRESS], 1);
    CHECK(counts[BTN_EVENT_REPEAT] >= 2U);
    CHECK_EQ_STR(btn_event_name(BTN_EVENT_LONG_PRESS), "long");
}

static uint32_t s_fired_a;
static uint32_t s_fired_b;
static void cb_a(void *arg) { (void)arg; s_fired_a++; }
static void cb_b(void *arg) { (void)arg; s_fired_b++; }

TEST(softtimer)
{
    softtimer_stop_all();
    const int32_t a = softtimer_start(0U, 100U, SOFTTIMER_PERIODIC, cb_a, NULL);
    const int32_t b = softtimer_start(0U, 250U, SOFTTIMER_ONE_SHOT, cb_b, NULL);
    CHECK(a >= 0);
    CHECK(b >= 0);
    for (uint32_t t = 0U; t <= 1000U; t++) {
        (void)softtimer_process(t);
    }
    CHECK_EQ_INT(s_fired_a, 10);
    CHECK_EQ_INT(s_fired_b, 1);
    CHECK(!softtimer_is_active(b));
    CHECK_EQ_INT(softtimer_remaining(a, 1050U), 50);
    /* wrap-around of the 32-bit ms counter */
    softtimer_stop_all();
    s_fired_a = 0U;
    (void)softtimer_start(0xFFFFFF00U, 0x200U, SOFTTIMER_ONE_SHOT, cb_a, NULL);
    (void)softtimer_process(0x000000F0U);
    CHECK_EQ_INT(s_fired_a, 0);
    (void)softtimer_process(0x00000100U);
    CHECK_EQ_INT(s_fired_a, 1);

    uint32_t last = 0U;
    uint32_t hits = 0U;
    for (uint32_t t = 0U; t < 1000U; t++) {
        hits += softtimer_elapsed(&last, t, 250U) ? 1U : 0U;
    }
    CHECK_EQ_INT(hits, 3);
}

enum { ST_OFF, ST_ON, ST_ERR, ST_COUNT };
enum { EV_TOGGLE, EV_FAULT, EV_RESET };
static uint32_t s_enter_on;
static void on_enter_on(fsm_t *m, fsm_event_t ev) { (void)m; (void)ev; s_enter_on++; }

TEST(fsm)
{
    static const fsm_transition_t table[] = {
        { ST_OFF,  EV_TOGGLE, ST_ON,  NULL },
        { ST_ON,   EV_TOGGLE, ST_OFF, NULL },
        { FSM_ANY, EV_FAULT,  ST_ERR, NULL },
        { ST_ERR,  EV_RESET,  ST_OFF, NULL },
    };
    static const fsm_state_info_t states[ST_COUNT] = {
        { "off", NULL, NULL }, { "on", on_enter_on, NULL }, { "error", NULL, NULL }
    };
    fsm_t m;
    fsm_init(&m, table, 4U, states, ST_COUNT, ST_OFF, NULL);
    CHECK(fsm_dispatch(&m, EV_TOGGLE));
    CHECK_EQ_INT(fsm_state(&m), ST_ON);
    CHECK_EQ_INT(s_enter_on, 1);
    CHECK(!fsm_dispatch(&m, EV_RESET));
    CHECK(fsm_dispatch(&m, EV_FAULT));
    CHECK_EQ_STR(fsm_state_name(&m, fsm_state(&m)), "error");
    CHECK(!fsm_dispatch(&m, EV_TOGGLE));
    CHECK(fsm_dispatch(&m, EV_RESET));
    CHECK_EQ_INT(fsm_state(&m), ST_OFF);
}

TEST(evq)
{
    evq_event_t slots[4];
    evq_t       q;
    evq_event_t e;
    CHECK(evq_init(&q, slots, 4U));
    for (uint16_t i = 0U; i < 4U; i++) {
        CHECK(evq_post(&q, i, (uint16_t)(i * 2U), i * 100U));
    }
    CHECK(!evq_post(&q, 9U, 0U, 0U));
    CHECK_EQ_INT(q.dropped, 1);
    CHECK(evq_get(&q, &e));
    CHECK_EQ_INT(e.type, 0);
    CHECK(evq_get(&q, &e));
    CHECK_EQ_INT(e.data, 100);
    CHECK_EQ_INT(evq_count(&q), 2);
}

TEST(rand)
{
    rand_xs32_t r;
    uint32_t    hist[6] = { 0 };
    rand_xs32_seed(&r, 1U);
    CHECK_EQ_INT(rand_xs32_next(&r), 270369U);   /* reference xorshift32 */
    for (uint32_t i = 0U; i < 60000U; i++) {
        const int32_t v = rand_xs32_range(&r, 1, 6);
        CHECK((v >= 1) && (v <= 6));
        hist[v - 1]++;
    }
    for (uint32_t i = 0U; i < 6U; i++) {
        CHECK((hist[i] > 9500U) && (hist[i] < 10500U));
    }
    /* LFSR16 has a maximal period of 65535 */
    uint16_t       st    = 1U;
    const uint16_t start = st;
    uint32_t       period = 0U;
    do {
        (void)rand_lfsr16_next(&st);
        period++;
    } while ((st != start) && (period < 70000U));
    CHECK_EQ_INT(period, 65535);
    uint8_t deck[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    uint32_t sum = 0U;
    rand_shuffle_u8(&r, deck, 10U);
    for (uint32_t i = 0U; i < 10U; i++) {
        sum += deck[i];
    }
    CHECK_EQ_INT(sum, 45);
}

TEST(framing)
{
    uint8_t enc[320];
    uint8_t dec[320];
    uint8_t src[300];

    const uint8_t a[] = { 0x11U, 0x22U, 0x00U, 0x33U };
    CHECK_EQ_INT(cobs_encode(a, sizeof(a), enc, sizeof(enc)), 5);
    const uint8_t a_ref[] = { 0x03U, 0x11U, 0x22U, 0x02U, 0x33U };
    CHECK(memcmp(enc, a_ref, 5U) == 0);
    CHECK_EQ_INT(cobs_decode(enc, 5U, dec, sizeof(dec)), 4);
    CHECK(memcmp(dec, a, 4U) == 0);
    /* long runs without zero, and all-zero payloads */
    for (uint32_t n = 0U; n < 300U; n++) {
        src[n] = (uint8_t)((n % 7U) == 3U ? 0U : (n + 1U));
    }
    for (size_t len = 0U; len <= 300U; len += 13U) {
        const size_t e = cobs_encode(src, len, enc, sizeof(enc));
        CHECK(memchr(enc, 0, e) == NULL);
        CHECK_EQ_INT(cobs_decode(enc, e, dec, sizeof(dec)), len);
        CHECK(memcmp(dec, src, len) == 0);
    }

    const uint8_t p[] = { 0x01U, SLIP_END, 0x02U, SLIP_ESC, 0x03U };
    const size_t  n   = slip_encode(p, sizeof(p), enc, sizeof(enc));
    CHECK_EQ_INT(n, 8);
    slip_rx_t rx;
    size_t    got = 0U;
    slip_rx_init(&rx, dec, sizeof(dec));
    for (size_t i = 0U; i < n; i++) {
        got = slip_rx_feed(&rx, enc[i]);
    }
    CHECK_EQ_INT(got, sizeof(p));
    CHECK(memcmp(dec, p, sizeof(p)) == 0);
}

TEST(stats)
{
    stats_t s;
    stats_reset(&s);
    const int32_t data[] = { 2, 4, 4, 4, 5, 5, 7, 9 };
    for (size_t i = 0U; i < 8U; i++) {
        stats_add(&s, data[i]);
    }
    CHECK_EQ_INT(stats_mean(&s), 5);
    CHECK_EQ_INT(stats_variance(&s), 4);
    CHECK_EQ_INT(stats_stddev(&s), 2);
    CHECK_EQ_INT(s.min, 2);
    CHECK_EQ_INT(s.max, 9);

    uint32_t bins[4];
    histo_t  h;
    histo_init(&h, bins, 4U, 0, 100);
    histo_add(&h, 10);
    histo_add(&h, 30);
    histo_add(&h, 99);
    histo_add(&h, 150);
    histo_add(&h, -1);
    CHECK_EQ_INT(bins[0], 1);
    CHECK_EQ_INT(bins[1], 1);
    CHECK_EQ_INT(bins[3], 1);
    CHECK_EQ_INT(h.over, 1);
    CHECK_EQ_INT(h.under, 1);
}

TEST(counter_and_io)
{
    counter_t c;
    counter_init(&c, 0, 255, 1, COUNTER_MODE_WRAP);
    for (int k = 0; k < 255; k++) {
        CHECK(!counter_step(&c));
    }
    CHECK_EQ_INT(counter_get(&c), 255);
    CHECK(counter_step(&c));
    CHECK_EQ_INT(counter_get(&c), 0);

    counter_init(&c, 0, 10, 3, COUNTER_MODE_WRAP);
    for (int k = 0; k < 4; k++) {
        (void)counter_step(&c);
    }
    CHECK_EQ_INT(counter_get(&c), 1);    /* 0 3 6 9 -> 12 wraps to 1 */

    counter_init(&c, 0, 3, 1, COUNTER_MODE_BOUNCE);
    int32_t seq[8];
    for (int k = 0; k < 8; k++) {
        (void)counter_step(&c);
        seq[k] = counter_get(&c);
    }
    CHECK_EQ_INT(seq[2], 3);
    CHECK_EQ_INT(seq[3], 3);
    CHECK_EQ_INT(seq[4], 2);

    counter_init(&c, -5, 5, -2, COUNTER_MODE_SATURATE);
    for (int k = 0; k < 10; k++) {
        (void)counter_step(&c);
    }
    CHECK_EQ_INT(counter_get(&c), -5);

    counter_ext16_t e;
    counter_ext16_init(&e, 65000U);
    CHECK_EQ_INT(counter_ext16_update(&e, 100U), 65636U);

    io_init(capture_putc, NULL);
    capture_reset();
    counter_print_binary(5U, 8U);
    CHECK_EQ_STR(s_out, "binary counter: 00000101\r\n");
    capture_reset();
    counter_print_hex(0xABU, 2U);
    CHECK_EQ_STR(s_out, "hex counter : 0xAB\r\n");
    capture_reset();
    counter_print_decimal(7U, 3U);
    CHECK_EQ_STR(s_out, "decimal count: 007\r\n");
    capture_reset();
    io_printf("%u-%s", 42U, "ok");
    CHECK_EQ_STR(s_out, "42-ok");
    CHECK_EQ_INT(io_getc(), IO_EOF);
}


TEST(nmea)
{
    nmea_t  n;
    int32_t v = 0;
    char    line[NMEA_MAX_SENTENCE];

    CHECK(nmea_parse_coord("4807.038", 'N', &v) && (v == 48117300));
    CHECK(nmea_parse_coord("01131.000", 'E', &v) && (v == 11516667));
    CHECK(nmea_parse_coord("4714.268", 'S', &v) && (v == -47237800));
    CHECK(!nmea_parse_coord("47x4.2", 'N', &v));
    CHECK(!nmea_parse_coord("4714.2", 'Q', &v));
    CHECK(nmea_parse_fixed("022.4", 3U, &v) && (v == 22400));
    CHECK(nmea_parse_fixed("1.23456", 2U, &v) && (v == 123));
    CHECK(nmea_parse_fixed("1.235", 2U, &v) && (v == 124));
    CHECK(!nmea_parse_fixed("", 1U, &v));

    nmea_init(&n);
    const char *rmc = "garbage$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    nmea_sentence_t last = NMEA_SENTENCE_NONE;
    for (const char *p = rmc; *p != '\0'; p++) {
        const nmea_sentence_t t = nmea_feed(&n, *p);
        if (t != NMEA_SENTENCE_NONE) {
            last = t;
        }
    }
    CHECK_EQ_INT(last, NMEA_SENTENCE_RMC);
    CHECK(n.fix.valid);
    CHECK_EQ_INT(n.fix.lat_udeg, 48117300);
    CHECK_EQ_INT(n.fix.lon_udeg, 11516667);
    CHECK_EQ_INT(n.fix.course_cdeg, 8440);
    CHECK_NEAR_INT(n.fix.speed_mmps, 11523, 2);      /* 22.4 kn */
    CHECK((n.fix.hour == 12U) && (n.fix.minute == 35U) && (n.fix.second == 19U));
    CHECK((n.fix.day == 23U) && (n.fix.month == 3U) && (n.fix.year == 2094U));
    CHECK_EQ_INT(nmea_parse(&n, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6B"),
                 NMEA_SENTENCE_BAD_CRC);
    CHECK_EQ_INT(nmea_parse(&n, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"),
                 NMEA_SENTENCE_GGA);
    CHECK_EQ_INT(n.fix.satellites, 8);
    CHECK_EQ_INT(n.fix.alt_dm, 5454);
    CHECK_EQ_INT(n.fix.hdop_x100, 90);
    CHECK_EQ_INT(n.crc_errors, 1);

    /* round trip through the builder */
    nmea_fix_t f = n.fix;
    f.lat_udeg    = 47237800;
    f.lon_udeg    = -6024100;
    f.speed_mmps  = 5000U;
    f.course_cdeg = 27012U;
    const uint32_t len = nmea_format_rmc(line, sizeof(line), &f);
    CHECK(len > 40U);
    nmea_t back;
    nmea_init(&back);
    last = NMEA_SENTENCE_NONE;
    for (uint32_t i = 0U; i < len; i++) {
        const nmea_sentence_t t = nmea_feed(&back, line[i]);
        if (t != NMEA_SENTENCE_NONE) {
            last = t;
        }
    }
    CHECK_EQ_INT(last, NMEA_SENTENCE_RMC);
    CHECK_NEAR_INT(back.fix.lat_udeg, 47237800, 1);
    CHECK_NEAR_INT(back.fix.lon_udeg, -6024100, 1);
    CHECK_NEAR_INT(back.fix.speed_mmps, 5000, 2);
    CHECK_EQ_INT(back.fix.course_cdeg, 27012);
}

TEST(geo)
{
    const geo_point_t bes   = { 47237800, 6024100 };
    const geo_point_t gare  = { 47246600, 6021900 };
    const geo_point_t dijon = { 47322000, 5041500 };
    const geo_point_t munich = { 48117300, 11516667 };
    const geo_point_t munich_e = { 48117300, 11517667 };
    geo_point_t       moved;
    int32_t           e;
    int32_t           n;

    CHECK_NEAR_INT(geo_distance_m(&bes, &gare), 993, 3);
    CHECK_NEAR_INT(geo_bearing_cdeg(&bes, &gare), 35037, 30);
    CHECK_NEAR_INT(geo_distance_m(&bes, &dijon), 74713, 150);  /* 0.2 % at 75 km */
    CHECK_NEAR_INT(geo_bearing_cdeg(&bes, &dijon), 27756, 50);
    CHECK_NEAR_INT(geo_distance_m(&munich, &munich_e), 74, 1);
    CHECK_NEAR_INT(geo_bearing_cdeg(&munich, &munich_e), 9000, 5);
    CHECK_EQ_INT(geo_distance_m(&bes, &bes), 0);
    geo_move(&bes, 300, -400, &moved);
    geo_offset_m(&bes, &moved, &e, &n);
    CHECK_NEAR_INT(e, 300, 1);
    CHECK_NEAR_INT(n, -400, 1);
    CHECK_NEAR_INT(geo_distance_m(&bes, &moved), 500, 1);
    CHECK_EQ_STR(geo_compass(0U), "N");
    CHECK_EQ_STR(geo_compass(4500U), "NE");
    CHECK_EQ_STR(geo_compass(35000U), "N");
    CHECK_EQ_STR(geo_compass(20250U), "SSW");
    CHECK_EQ_INT(geo_turn_cdeg(35000U, 1000U), 2000);       /* 350 -> 10 : right 20 */
    CHECK_EQ_INT(geo_turn_cdeg(1000U, 35000U), -2000);
    /* route going north, rider 30 m east of it, half way */
    geo_point_t end;
    geo_point_t rider;
    geo_move(&bes, 0, 1000, &end);
    geo_move(&bes, 30, 500, &rider);
    CHECK_NEAR_INT(geo_cross_track_m(&bes, &end, &rider), 30, 1);
    CHECK_NEAR_INT(geo_along_track_permille(&bes, &end, &rider), 500, 2);
}

TEST(gfx)
{
    static uint8_t fb[128 * 64 / 8];
    gfx_t g;
    gfx_init(&g, fb, 128, 64);
    gfx_pixel(&g, 0, 0, GFX_SET);
    gfx_pixel(&g, 127, 63, GFX_SET);
    gfx_pixel(&g, 200, 10, GFX_SET);           /* clipped, no crash */
    CHECK(gfx_get_pixel(&g, 0, 0));
    CHECK(gfx_get_pixel(&g, 127, 63));
    CHECK_EQ_INT(fb[0], 0x01);
    CHECK_EQ_INT(fb[(7 * 128) + 127], 0x80);
    gfx_clear(&g);
    gfx_line(&g, 0, 0, 10, 5, GFX_SET);
    CHECK(gfx_get_pixel(&g, 10, 5));
    CHECK(gfx_get_pixel(&g, 0, 0));
    uint32_t count = 0U;
    for (int16_t x = 0; x < 128; x++) {
        for (int16_t y = 0; y < 64; y++) {
            count += gfx_get_pixel(&g, x, y) ? 1U : 0U;
        }
    }
    CHECK_EQ_INT(count, 11);                   /* one pixel per column */
    gfx_clear(&g);
    gfx_fill_rect(&g, 10, 10, 4, 3, GFX_SET);
    gfx_pixel(&g, 11, 11, GFX_INVERT);
    CHECK(!gfx_get_pixel(&g, 11, 11));
    CHECK(gfx_get_pixel(&g, 13, 12));
    CHECK(!gfx_get_pixel(&g, 14, 12));
    gfx_clear(&g);
    gfx_circle(&g, 32, 32, 10, GFX_SET);
    CHECK(gfx_get_pixel(&g, 42, 32) && gfx_get_pixel(&g, 32, 22) && gfx_get_pixel(&g, 22, 32));
    CHECK(!gfx_get_pixel(&g, 32, 32));
    gfx_clear(&g);
    gfx_char(&g, 0, 0, 'I', GFX_SET);          /* 0x00 0x41 0x7F 0x41 0x00 */
    CHECK_EQ_INT(fb[2], 0x7F);
    CHECK_EQ_INT(gfx_text_width(&g, "abc"), 17);
    gfx_set_text_scale(&g, 2U);
    CHECK_EQ_INT(gfx_text_width(&g, "ab"), 22);
}

typedef struct {
    int32_t     key;
    list_node_t node;
} item_t;

static int cmp_item(const list_node_t *a, const list_node_t *b)
{
    const int32_t x = LIST_ENTRY(a, item_t, node)->key;
    const int32_t y = LIST_ENTRY(b, item_t, node)->key;
    return (x > y) - (x < y);
}

TEST(list_pool_bitset)
{
    list_node_t head;
    item_t      items[5] = { { 5, { 0, 0 } }, { 1, { 0, 0 } }, { 4, { 0, 0 } }, { 1, { 0, 0 } }, { 3, { 0, 0 } } };
    list_init(&head);
    CHECK(list_is_empty(&head));
    for (int i = 0; i < 5; i++) {
        list_insert_sorted(&head, &items[i].node, cmp_item);
    }
    CHECK_EQ_INT(list_count(&head), 5);
    int32_t prev = -1000;
    LIST_FOR_EACH(it, &head) {
        const int32_t k = LIST_ENTRY(it, item_t, node)->key;
        CHECK(k >= prev);
        prev = k;
    }
    CHECK(LIST_ENTRY(head.next, item_t, node) == &items[1]);   /* stable */
    LIST_FOR_EACH_SAFE(it, tmp, &head) {
        if (LIST_ENTRY(it, item_t, node)->key == 1) {
            list_remove(it);
        }
    }
    CHECK_EQ_INT(list_count(&head), 3);
    CHECK(LIST_ENTRY(list_pop_front(&head), item_t, node)->key == 3);

    static void *storage[16];
    pool_t pool;
    CHECK(pool_init(&pool, storage, sizeof(storage), 12U));
    CHECK_EQ_INT(pool.block_size, POOL_BLOCK_SIZE(12U));
    void *blocks[20];
    uint32_t got = 0U;
    for (uint32_t i = 0U; i < 20U; i++) {
        blocks[i] = pool_alloc(&pool);
        got += (blocks[i] != NULL) ? 1U : 0U;
    }
    CHECK_EQ_INT(got, pool.total);
    CHECK_EQ_INT(pool.failures, 20U - pool.total);
    CHECK(blocks[0] == (void *)storage);
    CHECK(!pool_free(&pool, &got));                  /* foreign pointer */
    CHECK(pool_free(&pool, blocks[3]));
    CHECK(pool_alloc(&pool) == blocks[3]);           /* LIFO reuse */
    CHECK_EQ_INT(pool.peak, pool.total);

    BITSET_DEFINE(bs, 70);
    CHECK_EQ_INT(bitset_find_first_set(bs, 70U), -1);
    bitset_set(bs, 0U);
    bitset_set(bs, 33U);
    bitset_set(bs, 69U);
    CHECK_EQ_INT(bitset_count(bs, 70U), 3);
    CHECK_EQ_INT(bitset_find_next_set(bs, 70U, 1U), 33);
    CHECK_EQ_INT(bitset_find_next_set(bs, 70U, 34U), 69);
    CHECK_EQ_INT(bitset_find_first_zero(bs, 70U), 1);
    bitset_fill(bs, 70U, true);
    CHECK_EQ_INT(bitset_count(bs, 70U), 70);
    CHECK_EQ_INT(bitset_alloc(bs, 70U), -1);
    bitset_clear(bs, 40U);
    CHECK_EQ_INT(bitset_alloc(bs, 70U), 40);
    CHECK(bitset_test(bs, 40U));
}

TEST(sort)
{
    int32_t     a[200];
    int32_t     b[200];
    rand_xs32_t r;
    rand_xs32_seed(&r, 7U);
    for (uint32_t round = 0U; round < 20U; round++) {
        const size_t n = (size_t)rand_xs32_below(&r, 200U) + 1U;
        for (size_t i = 0U; i < n; i++) {
            a[i] = rand_xs32_range(&r, -50, 50);
            b[i] = a[i];
        }
        sort_i32(a, n);
        CHECK(sort_is_sorted(a, n, sizeof(int32_t), sort_cmp_i32));
        const size_t k = n / 3U;
        CHECK_EQ_INT(sort_i32_select(b, n, k), a[k]);
    }
    int32_t s[6] = { 9, -3, 7, 7, 0, 2 };
    sort_insertion(s, 6U, sizeof(int32_t), sort_cmp_i32);
    CHECK((s[0] == -3) && (s[5] == 9));
    const int32_t key = 7;
    CHECK_EQ_INT(sort_bsearch(&key, s, 6U, sizeof(int32_t), sort_cmp_i32) >= 3, 1);
    const int32_t missing = 5;
    CHECK_EQ_INT(sort_bsearch(&missing, s, 6U, sizeof(int32_t), sort_cmp_i32), -1);
    CHECK_EQ_INT(sort_lower_bound(&missing, s, 6U, sizeof(int32_t), sort_cmp_i32), 3);
    int32_t m[5] = { 5, 1, 4, 2, 3 };
    CHECK_EQ_INT(sort_i32_median(m, 5U), 3);
}

static uint8_t s_mb_tx[MODBUS_MAX_FRAME];
static size_t  s_mb_tx_len;
static void mb_send(const uint8_t *f, size_t len, void *ctx)
{
    (void)ctx;
    memcpy(s_mb_tx, f, len);
    s_mb_tx_len = len;
}

TEST(modbus)
{
    modbus_slave_t mb;
    uint16_t holding[10] = { 0 };
    uint16_t input[4]    = { 100, 200, 300, 400 };
    uint8_t  coils[2]    = { 0x05U, 0x00U };
    uint8_t  req[MODBUS_MAX_FRAME];

    modbus_init(&mb, 17U, mb_send, NULL);
    mb.holding = holding;  mb.holding_count = 10U;
    mb.input   = input;    mb.input_count   = 4U;
    mb.coils   = coils;    mb.coil_count    = 16U;
    modbus_set_baud(&mb, 9600U);
    CHECK_EQ_INT(mb.silence_us, 4010);

    /* read input registers 1..2 */
    size_t n = modbus_build_request(req, 17U, 0x04U, 1U, 2U);
    modbus_process(&mb, req, n);
    CHECK_EQ_INT(s_mb_tx_len, 9);
    CHECK((s_mb_tx[2] == 4U) && (s_mb_tx[3] == 0U) && (s_mb_tx[4] == 200U) && (s_mb_tx[6] == 44U));
    CHECK_EQ_INT(crc16_modbus(0xFFFFU, s_mb_tx, s_mb_tx_len), 0);   /* CRC residue */
    /* write single register, then read it back */
    n = modbus_build_request(req, 17U, 0x06U, 3U, 0xBEEFU);
    modbus_process(&mb, req, n);
    CHECK_EQ_INT(holding[3], 0xBEEF);
    CHECK(memcmp(s_mb_tx, req, n) == 0);           /* echo */
    /* write multiple: 0x10, addr 0, 2 regs */
    const uint8_t wm[] = { 17U, 0x10U, 0U, 0U, 0U, 2U, 4U, 0x12U, 0x34U, 0x56U, 0x78U };
    memcpy(req, wm, sizeof(wm));
    const uint16_t c = crc16_modbus(0xFFFFU, req, sizeof(wm));
    req[sizeof(wm)] = (uint8_t)c;
    req[sizeof(wm) + 1U] = (uint8_t)(c >> 8);
    modbus_process(&mb, req, sizeof(wm) + 2U);
    CHECK((holding[0] == 0x1234U) && (holding[1] == 0x5678U));
    CHECK_EQ_INT(s_mb_tx_len, 8);
    /* coils: read 4, write one on */
    n = modbus_build_request(req, 17U, 0x01U, 0U, 4U);
    modbus_process(&mb, req, n);
    CHECK_EQ_INT(s_mb_tx[3], 0x05);
    n = modbus_build_request(req, 17U, 0x05U, 9U, 0xFF00U);
    modbus_process(&mb, req, n);
    CHECK_EQ_INT(coils[1], 0x02);
    /* exceptions */
    n = modbus_build_request(req, 17U, 0x03U, 9U, 5U);
    modbus_process(&mb, req, n);
    CHECK((s_mb_tx[1] == 0x83U) && (s_mb_tx[2] == MODBUS_EX_ILLEGAL_ADDR));
    n = modbus_build_request(req, 17U, 0x2BU, 0U, 1U);
    modbus_process(&mb, req, n);
    CHECK((s_mb_tx[1] == 0xABU) && (s_mb_tx[2] == MODBUS_EX_ILLEGAL_FUNC));
    /* other slave address and bad CRC: silence */
    s_mb_tx_len = 0U;
    n = modbus_build_request(req, 18U, 0x03U, 0U, 1U);
    modbus_process(&mb, req, n);
    req[2] ^= 1U;
    modbus_process(&mb, req, n);
    CHECK_EQ_INT(s_mb_tx_len, 0);
    CHECK_EQ_INT(mb.crc_errors, 1);
    /* byte stream with inter-frame silence */
    n = modbus_build_request(req, 17U, 0x03U, 0U, 1U);
    uint32_t t = 1000U;
    for (size_t i = 0U; i < n; i++) {
        modbus_rx_byte(&mb, req[i], t);
        t += 1000U;
    }
    CHECK(!modbus_poll(&mb, t));
    CHECK(modbus_poll(&mb, t + 5000U));
    CHECK((s_mb_tx[3] == 0x12U) && (s_mb_tx[4] == 0x34U));
}

static void draw_line(uint32_t row, const char *text, bool selected, void *ctx)
{
    char *screen = (char *)ctx;
    fmt_snprintf(&screen[row * 32U], 32U, "%c%s", selected ? '>' : ' ', text);
}

static uint32_t s_action_runs;
static void menu_action(void) { s_action_runs++; }

TEST(dds_trace_menu)
{
    CHECK_EQ_INT(dds_tuning_word(100U, 100U), 42949673U);            /* 1 Hz at 100 Hz */
    dds_t d;
    dds_init(&d, 1000U, DDS_SQUARE, 1000U, 0U);
    dds_set_freq_c(&d, 10000U);                                      /* 100 Hz */
    uint32_t highs = 0U;
    for (uint32_t i = 0U; i < 1000U; i++) {
        highs += (dds_next(&d) > 500U) ? 1U : 0U;
    }
    CHECK_NEAR_INT(highs, 500, 10);
    dds_init(&d, 8000U, DDS_SINE, 2047U, 2048U);
    dds_set_freq_c(&d, 100000U);                                     /* 1 kHz, 8 samples/period */
    CHECK_NEAR_INT(dds_next(&d), 2048, 2);
    CHECK_NEAR_INT(dds_next(&d), 2048 + 1447, 4);
    CHECK_NEAR_INT(dds_next(&d), 4095, 3);

    trace_init(NULL);
    trace_clear();
    for (uint32_t i = 0U; i < (TRACE_DEPTH + 5U); i++) {
        TRACE(1, i);
    }
    trace_event_t ev;
    CHECK_EQ_INT(trace_count(), TRACE_DEPTH);
    CHECK(trace_get(0U, &ev) && (ev.value == 5U));                  /* oldest kept */
    CHECK(trace_get(TRACE_DEPTH - 1U, &ev) && (ev.value == (TRACE_DEPTH + 4U)));

    static int32_t level = 50;
    static bool    beep  = false;
    static const menu_item_t sub[] = {
        MENU_VALUE("Level", &level, 0, 60, 5, "%"),
        MENU_TOGGLE("Beep", &beep),
        MENU_END
    };
    static const menu_item_t root[] = {
        MENU_SUB("Settings", sub),
        MENU_ACTION("Run", menu_action),
        MENU_ACTION("Stop", menu_action),
        MENU_END
    };
    char   screen[2 * 32];
    menu_t m;
    menu_init(&m, root, 2U, draw_line, screen);
    menu_render(&m);
    CHECK_EQ_STR(&screen[0], ">Settings >");
    CHECK_EQ_STR(&screen[32], " Run");
    (void)menu_key(&m, MENU_KEY_DOWN);
    (void)menu_key(&m, MENU_KEY_DOWN);
    menu_render(&m);
    CHECK_EQ_STR(&screen[32], ">Stop");                              /* scrolled */
    (void)menu_key(&m, MENU_KEY_SELECT);
    CHECK_EQ_INT(s_action_runs, 1);
    (void)menu_key(&m, MENU_KEY_DOWN);                               /* wraps to Settings */
    (void)menu_key(&m, MENU_KEY_SELECT);                             /* enter submenu */
    (void)menu_key(&m, MENU_KEY_SELECT);                             /* edit Level */
    (void)menu_key(&m, MENU_KEY_UP);
    (void)menu_key(&m, MENU_KEY_UP);
    (void)menu_key(&m, MENU_KEY_UP);                                 /* clamps at 60 */
    menu_render(&m);
    CHECK_EQ_STR(&screen[0], ">Level [60%]");
    (void)menu_key(&m, MENU_KEY_SELECT);                             /* end edit */
    (void)menu_key(&m, MENU_KEY_DOWN);
    (void)menu_key(&m, MENU_KEY_SELECT);                             /* toggle */
    CHECK(beep);
    CHECK(menu_key(&m, MENU_KEY_BACK));
    CHECK(!menu_key(&m, MENU_KEY_BACK));                             /* already at root */
    CHECK_EQ_INT(level, 60);
}

TEST(dsp)
{
    enum { LOG2N = 7, N = 1 << LOG2N, FS = 8000 };
    static int16_t    sig[N];
    static dsp_cplx_t work[N];
    static uint16_t   mag[(N / 2) + 1];

    /* 1 kHz tone (bin 16) amplitude 10000 + DC 500 */
    for (uint32_t i = 0U; i < N; i++) {
        sig[i] = (int16_t)(500 + ((10000 * fx_sin_q15((uint16_t)(((uint64_t)i * 65536U * 1000U) / FS))) / 32767));
    }
    CHECK(dsp_fft_magnitude(sig, LOG2N, work, mag));
    uint16_t peak;
    const uint32_t k = dsp_peak(mag, 1U, (N / 2) + 1, &peak);
    CHECK_EQ_INT(k, 16);
    CHECK_NEAR_INT(peak, 10000, 150);
    CHECK_NEAR_INT(mag[0], 500, 20);
    CHECK(mag[40] < 100U);                                /* no leakage far away */
    CHECK_EQ_INT(dsp_bin_freq_c(k, FS, N), 100000);
    CHECK(!dsp_fft(work, 0U));

    CHECK_NEAR_INT(dsp_mean(sig, N), 500, 2);
    CHECK_NEAR_INT(dsp_zero_cross_freq_c(sig, N, FS), 100000, 1500);   /* offset biases it a bit */
    dsp_remove_dc(sig, N);
    CHECK_NEAR_INT(dsp_mean(sig, N), 0, 1);
    CHECK_NEAR_INT(dsp_rms(sig, N), 7071, 30);              /* A / sqrt 2 */
    CHECK_NEAR_INT(dsp_zero_cross_freq_c(sig, N, FS), 100000, 300);
    int16_t lo;
    int16_t hi;
    dsp_minmax(sig, N, &lo, &hi);
    CHECK_NEAR_INT(hi, 10000, 60);
    CHECK_NEAR_INT(lo, -10000, 60);

    /* Goertzel: strong at 1 kHz, nothing at 2.5 kHz */
    const uint32_t p1 = dsp_goertzel(sig, N, 1000U, FS);
    const uint32_t p2 = dsp_goertzel(sig, N, 2500U, FS);
    CHECK_NEAR_INT(p1, 100000000, 3000000);                 /* ~A^2 */
    CHECK(p2 < (p1 / 1000U));

    /* Hann window: ends go to zero, middle kept */
    int16_t w[9] = { 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000 };
    dsp_window_hann(w, 9U);
    CHECK_EQ_INT(w[0], 0);
    CHECK_NEAR_INT(w[4], 1000, 2);
    CHECK_NEAR_INT(w[2], 500, 3);
}

static char    s_scpi_out[256];
static int32_t s_volt_mv[3];
static bool    s_output_on;

static void scpi_out(const char *t, size_t len, void *ctx)
{
    (void)ctx;
    strncat(s_scpi_out, t, len);
}

static int32_t h_meas(scpi_t *s, scpi_args_t *a)
{
    int32_t ch = 1;
    if (!scpi_args_empty(a) && !scpi_param_fixed(a, 0U, 1, 2, 1, &ch)) {
        return SCPI_ERR_DATA_OUT_OF_RANGE;
    }
    scpi_result_fixed(s, s_volt_mv[ch] * 10, 4U);
    return SCPI_ERR_NONE;
}

static int32_t h_sour(scpi_t *s, scpi_args_t *a)
{
    int32_t mv;
    (void)s;
    if (a->suffix[0] > 2) {
        return SCPI_ERR_SUFFIX_RANGE;
    }
    if (scpi_args_empty(a)) {
        return SCPI_ERR_MISSING_PARAM;
    }
    if (!scpi_param_fixed(a, 3U, 0, 3300, 0, &mv)) {
        return SCPI_ERR_DATA_OUT_OF_RANGE;
    }
    s_volt_mv[a->suffix[0]] = mv;
    return SCPI_ERR_NONE;
}

static int32_t h_sour_q(scpi_t *s, scpi_args_t *a)
{
    scpi_result_fixed(s, s_volt_mv[a->suffix[0]], 3U);
    return SCPI_ERR_NONE;
}

static int32_t h_outp(scpi_t *s, scpi_args_t *a)
{
    (void)s;
    return scpi_param_bool(a, &s_output_on) ? SCPI_ERR_NONE : SCPI_ERR_DATA_TYPE;
}

TEST(scpi)
{
    int32_t  sfx[3];
    uint32_t ns;
    CHECK(scpi_match("MEASure:VOLTage[:DC]?", "MEAS:VOLT?", 10U, sfx, &ns));
    CHECK(scpi_match("MEASure:VOLTage[:DC]?", "measure:voltage:dc?", 19U, sfx, &ns));
    CHECK(scpi_match("MEASure:VOLTage[:DC]?", ":Meas:Volt:DC?", 14U, sfx, &ns));
    CHECK(!scpi_match("MEASure:VOLTage[:DC]?", "MEASU:VOLT?", 11U, sfx, &ns));     /* partial */
    CHECK(!scpi_match("MEASure:VOLTage[:DC]?", "MEAS:VOLT", 9U, sfx, &ns));        /* not a query */
    CHECK(scpi_match("SOURce#:VOLTage[:LEVel]", "SOUR2:VOLT", 10U, sfx, &ns) && (ns == 1U) && (sfx[0] == 2));
    CHECK(scpi_match("SOURce#:VOLTage[:LEVel]", "SOUR:VOLT:LEV", 13U, sfx, &ns) && (sfx[0] == 1));
    CHECK(!scpi_match("OUTPut[:STATe]", "OUTP3", 5U, sfx, &ns));                     /* no suffix allowed */

    static const scpi_command_t cmds[] = {
        { "MEASure:VOLTage[:DC]?",   h_meas },
        { "SOURce#:VOLTage[:LEVel]",  h_sour },
        { "SOURce#:VOLTage[:LEVel]?", h_sour_q },
        { "OUTPut[:STATe]",           h_outp },
    };
    scpi_t sc;
    scpi_init(&sc, cmds, 4U, "ELISE,TEST,0,1.0", scpi_out, NULL);
    s_scpi_out[0] = '\0';
    scpi_input(&sc, "*IDN?\n", 6U);
    CHECK_EQ_STR(s_scpi_out, "ELISE,TEST,0,1.0\n");

    s_scpi_out[0] = '\0';
    scpi_execute(&sc, "SOUR1:VOLT 1.25;SOUR2:VOLT 250mV;:sour1:volt?;SOUR2:VOLT?");
    CHECK_EQ_STR(s_scpi_out, "1.250;0.250\n");
    s_scpi_out[0] = '\0';
    scpi_execute(&sc, "SOUR1:VOLT 2.5e-3 V; SOUR1:VOLT?");
    CHECK_EQ_STR(s_scpi_out, "0.003\n");
    s_scpi_out[0] = '\0';
    scpi_execute(&sc, "SOUR1:VOLT MAX;SOUR1:VOLT?;MEAS:VOLT? 1");
    CHECK_EQ_STR(s_scpi_out, "3.300;3.3000\n");
    scpi_execute(&sc, "OUTP ON");
    CHECK(s_output_on);
    scpi_execute(&sc, "OUTPut:STATe off");
    CHECK(!s_output_on);

    /* errors go to the queue, read with SYST:ERR? */
    scpi_execute(&sc, "FOO:BAR");
    scpi_execute(&sc, "SOUR1:VOLT 5");
    scpi_execute(&sc, "SOUR3:VOLT 1");
    scpi_execute(&sc, "SOUR1:VOLT");
    s_scpi_out[0] = '\0';
    scpi_execute(&sc, "SYST:ERR:COUN?;SYST:ERR?;SYST:ERR?;SYST:ERR?;SYST:ERR?;SYST:ERR?");
    CHECK_EQ_STR(s_scpi_out, "4;-113,\"Undefined header\";-222,\"Data out of range\";"
                             "-114,\"Header suffix out of range\";-109,\"Missing parameter\";0,\"No error\"\n");
    s_scpi_out[0] = '\0';
    scpi_execute(&sc, "*ESR?");
    CHECK_EQ_STR(s_scpi_out, "32\n");
    scpi_execute(&sc, "*CLS");
    CHECK_EQ_INT(sc.err_count, 0);
    for (int i = 0; i < 12; i++) {
        scpi_execute(&sc, "BAD");
    }
    CHECK_EQ_INT(sc.err_count, SCPI_ERROR_QUEUE);
    for (uint32_t i = 0U; i < (SCPI_ERROR_QUEUE - 1U); i++) {
        (void)scpi_error_pop(&sc);
    }
    CHECK_EQ_INT(scpi_error_pop(&sc), SCPI_ERR_QUEUE_OVERFLOW);

    scpi_args_t a;
    int32_t     v;
    a.text = " 2.5kHz";
    CHECK(scpi_param_fixed(&a, 0U, 0, 100000, 0, &v) && (v == 2500));
    a.text = "1MHZ";
    CHECK(scpi_param_fixed(&a, 0U, 0, 10000000, 0, &v) && (v == 1000000));
    a.text = "-12.345";
    CHECK(scpi_param_fixed(&a, 2U, -10000, 10000, 0, &v) && (v == -1235));
    a.text = "abc";
    CHECK(!scpi_param_fixed(&a, 0U, 0, 10, 0, &v));
    static const char *const shapes[] = { "SINusoid", "SQUare", "TRIangle" };
    a.text = "squ";
    CHECK_EQ_INT(scpi_param_choice(&a, shapes, 3U), 1);
    a.text = "SINUSOID, 3";
    CHECK_EQ_INT(scpi_param_choice(&a, shapes, 3U), 0);
    CHECK(scpi_param_fixed(&a, 0U, 0, 10, 0, &v) && (v == 3));
    char str[16];
    a.text = "\"hello world\"";
    CHECK(scpi_param_string(&a, str, sizeof(str)));
    CHECK_EQ_STR(str, "hello world");
}

int main(void)
{
    printf("core unit tests\n");
    RUN(bits);
    RUN(ringbuf);
    RUN(fmt);
    RUN(strutil);
    RUN(codes);
    RUN(crc);
    RUN(fixmath);
    RUN(filter);
    RUN(pid);
    RUN(debounce);
    RUN(softtimer);
    RUN(fsm);
    RUN(evq);
    RUN(rand);
    RUN(framing);
    RUN(stats);
    RUN(counter_and_io);
    RUN(nmea);
    RUN(geo);
    RUN(gfx);
    RUN(list_pool_bitset);
    RUN(sort);
    RUN(modbus);
    RUN(dds_trace_menu);
    RUN(dsp);
    RUN(scpi);
    return unit_summary();
}

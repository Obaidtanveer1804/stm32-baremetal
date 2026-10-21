/**
 * @file    cmd_util.c
 * @brief   Pure software tools, handy during labs (no hardware needed):
 *
 *   calc (1<<8)|(1<<9)         C expression, result in dec/hex/bin
 *   let psc 71 ; calc 72000000/(psc+1)
 *   conv 0xA5                  every representation of a number
 *   bits 0x300                 which bits are set
 *   crc 31 32 33               CRC8/16/32 of bytes   (crc "text")
 *   morse SOS PC9              blink a message
 *   seg7 42                    7-segment patterns
 *   rand 1 6 [n]               dice
 *   b64 hello / b64 -d aGVsbG8=
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "bits.h"
#include "codes.h"
#include "crc.h"
#include "expr.h"
#include "rand.h"
#include "strutil.h"

#define VAR_MAX        (8U)
#define VAR_NAME_MAX   (8U)
#define MORSE_UNIT_MS  (120U)     /* dot length (~10 words/min) */

typedef struct {
    char    name[VAR_NAME_MAX];
    int32_t value;
} var_t;

static var_t s_vars[VAR_MAX];

static bool var_lookup(const char *name, uint32_t len, int32_t *value)
{
    if (len == 3U && (str_ncmp(name, "ans", 3U) == 0)) {
        *value = s_vars[0].value;          /* slot 0 = last result */
        return true;
    }
    for (uint32_t i = 1U; i < VAR_MAX; i++) {
        if ((s_vars[i].name[0] != '\0') && (str_len(s_vars[i].name) == len) &&
            (str_ncmp(s_vars[i].name, name, len) == 0)) {
            *value = s_vars[i].value;
            return true;
        }
    }
    return false;
}

static void print_number(int32_t v)
{
    const uint32_t u = (uint32_t)v;
    io_printf("%ld  0x%08lX  %lu" IO_NEWLINE, (long)v, (unsigned long)u, (unsigned long)u);
    io_puts("  bin ");
    for (int32_t b = 31; b >= 0; b--) {
        io_putc(((u >> (uint32_t)b) & 1U) ? '1' : '0');
        if (((b % 8) == 0) && (b != 0)) {
            io_putc(' ');
        }
    }
    io_puts(IO_NEWLINE);
}

/* Glue argv back into one string (spaces in expressions are allowed). */
static void join_args(int argc, char *argv[], int first, char *buf, size_t size)
{
    buf[0] = '\0';
    for (int i = first; i < argc; i++) {
        (void)str_cat(buf, argv[i], size);
        (void)str_cat(buf, " ", size);
    }
}

static int eval_args(int argc, char *argv[], int first, int32_t *v)
{
    char text[CLI_LINE_MAX];
    join_args(argc, argv, first, text, sizeof(text));
    const expr_status_t st = expr_eval(text, v, var_lookup);
    if (st != EXPR_OK) {
        io_printf("  %s" IO_NEWLINE "  ", text);
        io_repeat(' ', expr_error_pos());
        io_println("^");
        return cli_error("%s", expr_status_str(st));
    }
    return CLI_OK;
}

static int cmd_calc(int argc, char *argv[])
{
    int32_t v;
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    if (eval_args(argc, argv, 1, &v) != CLI_OK) {
        return CLI_ERR;
    }
    s_vars[0].value = v;
    print_number(v);
    return CLI_OK;
}

static int cmd_let(int argc, char *argv[])
{
    int32_t v;
    if (argc == 1) {
        for (uint32_t i = 1U; i < VAR_MAX; i++) {
            if (s_vars[i].name[0] != '\0') {
                io_printf("%-8s = %ld (0x%lX)" IO_NEWLINE, s_vars[i].name, (long)s_vars[i].value,
                          (unsigned long)(uint32_t)s_vars[i].value);
            }
        }
        return CLI_OK;
    }
    if ((argc < 3) || !chr_is_alpha(argv[1][0]) || (str_len(argv[1]) >= VAR_NAME_MAX)) {
        return CLI_ERR_USAGE;
    }
    if (eval_args(argc, argv, 2, &v) != CLI_OK) {
        return CLI_ERR;
    }
    uint32_t slot = 0U;
    for (uint32_t i = 1U; i < VAR_MAX; i++) {
        if (str_eq(s_vars[i].name, argv[1])) {
            slot = i;
            break;
        }
        if ((slot == 0U) && (s_vars[i].name[0] == '\0')) {
            slot = i;
        }
    }
    if (slot == 0U) {
        return cli_error("no free variable slot (%lu max)", (unsigned long)(VAR_MAX - 1U));
    }
    (void)str_copy(s_vars[slot].name, argv[1], VAR_NAME_MAX);
    s_vars[slot].value = v;
    return CLI_OK;
}

static int cmd_conv(int argc, char *argv[])
{
    int32_t v;
    if ((argc < 2) || (eval_args(argc, argv, 1, &v) != CLI_OK)) {
        return (argc < 2) ? CLI_ERR_USAGE : CLI_ERR;
    }
    const uint32_t u = (uint32_t)v;
    print_number(v);
    io_printf("  oct  %lo" IO_NEWLINE, (unsigned long)u);
    io_printf("  bcd  0x%lX" IO_NEWLINE, (unsigned long)codes_bin_to_bcd(u));
    io_printf("  gray 0x%lX" IO_NEWLINE, (unsigned long)codes_bin_to_gray(u));
    io_printf("  bswap 0x%08lX, bit-reversed 0x%08lX" IO_NEWLINE, (unsigned long)bits_bswap32(u),
              (unsigned long)bits_reverse32(u));
    io_printf("  popcount %lu, parity %lu, msb %ld, next pow2 %lu" IO_NEWLINE,
              (unsigned long)bits_popcount(u), (unsigned long)bits_parity(u),
              (long)bits_msb_index(u), (unsigned long)bits_next_pow2(u));
    if (chr_is_print((char)u) && (u < 0x80U)) {
        io_printf("  char '%c'" IO_NEWLINE, (char)u);
    }
    return CLI_OK;
}

static int cmd_bits(int argc, char *argv[])
{
    int32_t v;
    if ((argc < 2) || (eval_args(argc, argv, 1, &v) != CLI_OK)) {
        return (argc < 2) ? CLI_ERR_USAGE : CLI_ERR;
    }
    uint32_t u = (uint32_t)v;
    io_puts("bits set:");
    if (u == 0U) {
        io_puts(" none");
    }
    /* iterate set bits only: clear the lowest one each time */
    while (u != 0U) {
        io_printf(" %lu", (unsigned long)bits_ctz(u));
        u = bits_clear_lowest(u);
    }
    io_puts(IO_NEWLINE "  as C: ");
    u = (uint32_t)v;
    bool first = true;
    while (u != 0U) {
        io_printf("%s(1U << %lu)", first ? "" : " | ", (unsigned long)bits_ctz(u));
        first = false;
        u = bits_clear_lowest(u);
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

/* bytes from numbers, or from one quoted string */
static size_t collect_bytes(int argc, char *argv[], uint8_t *buf, size_t max)
{
    size_t n = 0U;
    for (int i = 1; (i < argc) && (n < max); i++) {
        uint32_t v;
        if (str_to_u32(argv[i], &v) && (v <= 0xFFU)) {
            buf[n++] = (uint8_t)v;
        } else {
            for (const char *p = argv[i]; (*p != '\0') && (n < max); p++) {
                buf[n++] = (uint8_t)*p;
            }
        }
    }
    return n;
}

static int cmd_crc(int argc, char *argv[])
{
    uint8_t buf[CLI_LINE_MAX];
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    const size_t n = collect_bytes(argc, argv, buf, sizeof(buf));
    io_printf("%lu bytes" IO_NEWLINE, (unsigned long)n);
    io_printf("  crc8 maxim   0x%02X" IO_NEWLINE, crc8_maxim(CRC8_MAXIM_INIT, buf, n));
    io_printf("  crc8 smbus   0x%02X" IO_NEWLINE, crc8_smbus(CRC8_SMBUS_INIT, buf, n));
    io_printf("  crc16 ccitt  0x%04X" IO_NEWLINE, crc16_ccitt(CRC16_CCITT_INIT, buf, n));
    io_printf("  crc16 modbus 0x%04X" IO_NEWLINE, crc16_modbus(CRC16_MODBUS_INIT, buf, n));
    io_printf("  crc16 xmodem 0x%04X" IO_NEWLINE, crc16_xmodem(CRC16_XMODEM_INIT, buf, n));
    io_printf("  crc32        0x%08lX" IO_NEWLINE, (unsigned long)crc32(buf, n));
    io_printf("  sum8 0x%02X  xor8 0x%02X  fletcher16 0x%04X  adler32 0x%08lX" IO_NEWLINE,
              checksum_sum8(buf, n), checksum_xor8(buf, n), checksum_fletcher16(buf, n),
              (unsigned long)checksum_adler32(buf, n));
    return CLI_OK;
}

static void morse_on(hal_pin_t pin, uint32_t units)
{
    if (pin != HAL_PIN_NONE) {
        hal_gpio_set(pin);
    }
    hal_delay_ms(units * MORSE_UNIT_MS);
    if (pin != HAL_PIN_NONE) {
        hal_gpio_clear(pin);
    }
}

static int cmd_morse(int argc, char *argv[])
{
    hal_pin_t pin = HAL_PIN_NONE;
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && hal_gpio_parse(argv[argc - 1], &pin)) {
        hal_gpio_mode(pin, HAL_GPIO_OUTPUT);
        argc--;
    }
    for (int w = 1; w < argc; w++) {
        for (const char *p = argv[w]; *p != '\0'; p++) {
            uint8_t pattern;
            const uint8_t len = codes_morse(*p, &pattern);
            for (uint8_t i = 0U; i < len; i++) {
                const bool dash = ((pattern >> i) & 1U) != 0U;
                io_putc(dash ? '-' : '.');
                morse_on(pin, dash ? 3U : 1U);      /* dash = 3 dots */
                hal_delay_ms(MORSE_UNIT_MS);        /* gap inside a letter */
            }
            io_putc(' ');
            hal_delay_ms(2U * MORSE_UNIT_MS);       /* letter gap = 3 units */
            if (cli_abort_requested()) {
                io_puts(IO_NEWLINE);
                return CLI_OK;
            }
        }
        io_puts("/ ");
        hal_delay_ms(4U * MORSE_UNIT_MS);           /* word gap = 7 units */
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

static int cmd_seg7(int argc, char *argv[])
{
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    const char *text = argv[1];
    const size_t n   = str_len(text);
    /* draw the digits side by side, 3 text rows */
    for (uint32_t row = 0U; row < 3U; row++) {
        for (size_t i = 0U; i < n; i++) {
            const uint8_t s = codes_seg7_char(text[i]);
            if (row == 0U) {
                io_printf(" %c  ", (s & SEG7_A) ? '_' : ' ');
            } else if (row == 1U) {
                io_printf("%c%c%c ", (s & SEG7_F) ? '|' : ' ', (s & SEG7_G) ? '_' : ' ', (s & SEG7_B) ? '|' : ' ');
            } else {
                io_printf("%c%c%c ", (s & SEG7_E) ? '|' : ' ', (s & SEG7_D) ? '_' : ' ', (s & SEG7_C) ? '|' : ' ');
            }
        }
        io_puts(IO_NEWLINE);
    }
    io_puts("gfedcba:");
    for (size_t i = 0U; i < n; i++) {
        io_printf(" 0x%02X", codes_seg7_char(text[i]));
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

static int cmd_rand(int argc, char *argv[])
{
    int32_t  lo = 0;
    int32_t  hi = 0x7FFFFFFF;
    uint32_t n  = 1U;
    static bool seeded;
    if (!seeded) {
        rand_seed(hal_time_us() ^ ((uint32_t)hal_adc_read(HAL_ADC_CH_TEMP) << 16));
        seeded = true;
    }
    if ((argc >= 3) && (!cli_arg_i32(argv[1], &lo) || !cli_arg_i32(argv[2], &hi))) {
        return CLI_ERR;
    }
    if ((argc >= 4) && !cli_arg_u32_range(argv[3], 1U, 1000U, &n)) {
        return CLI_ERR;
    }
    rand_xs32_t r;
    rand_xs32_seed(&r, rand_u32());
    for (uint32_t i = 0U; i < n; i++) {
        io_printf("%ld ", (long)rand_xs32_range(&r, lo, hi));
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

static int cmd_b64(int argc, char *argv[])
{
    uint8_t raw[CLI_LINE_MAX];
    char    text[(CLI_LINE_MAX * 4U / 3U) + 4U];
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    if (str_eq(argv[1], "-d")) {
        if (argc != 3) {
            return CLI_ERR_USAGE;
        }
        const size_t n = codes_base64_decode(argv[2], raw, sizeof(raw) - 1U);
        if (n == 0U) {
            return cli_error("invalid base64");
        }
        raw[n] = 0U;
        io_printf("%s" IO_NEWLINE, (const char *)raw);
        return CLI_OK;
    }
    const size_t n = collect_bytes(argc, argv, raw, sizeof(raw));
    (void)codes_base64_encode(raw, n, text, sizeof(text));
    io_println(text);
    return CLI_OK;
}

static const cli_command_t util_cmds[] = {
    { "calc",  cmd_calc,  "<expr>",          "C integer expression (ans = last result)", NULL },
    { "let",   cmd_let,   "[name <expr>]",   "variables for calc",                     NULL },
    { "conv",  cmd_conv,  "<expr>",          "dec/hex/bin/oct/bcd/gray/... of a value", NULL },
    { "bits",  cmd_bits,  "<expr>",          "list set bits, as C shifts",             NULL },
    { "crc",   cmd_crc,   "<bytes|text...>", "CRCs and checksums",                     NULL },
    { "morse", cmd_morse, "<text...> [pin]", "print / blink Morse code",               NULL },
    { "seg7",  cmd_seg7,  "<text>",          "7-segment patterns",                     NULL },
    { "rand",  cmd_rand,  "[lo hi [n]]",     "random numbers (xorshift32)",            NULL },
    { "b64",   cmd_b64,   "[-d] <text>",     "base64 encode / decode",                 NULL },
};

static const cli_group_t util_group = { "tools", util_cmds, sizeof(util_cmds) / sizeof(util_cmds[0]) };

const cli_group_t *cmd_util_group(void)
{
    return &util_group;
}

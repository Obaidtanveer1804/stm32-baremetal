/**
 * @file    scpi.c
 * @brief   SCPI parser (see scpi.h).
 */
#include "scpi.h"
#include "strutil.h"
#include "fmt.h"

#define SCPI_MAX_NODES  (8U)

typedef struct {
    const char *word;        /* letters of the node            */
    uint32_t    len;
    bool        optional;    /* [node] in the pattern          */
    bool        has_suffix;  /* '#' (pattern) or digits (header) */
    int32_t     suffix;
} node_t;

/* ------------------------------------------------------------------ */
/* Pattern matching                                                    */
/* ------------------------------------------------------------------ */

/* Split "SOURce#:VOLTage[:LEVel]?" or "sour2:volt?" into nodes. */
static uint32_t split_nodes(const char *s, size_t len, bool is_pattern, node_t *nodes, bool *query)
{
    uint32_t n = 0U;
    size_t   i = 0U;

    *query = (len > 0U) && (s[len - 1U] == '?');
    if (*query) {
        len--;
    }
    if ((len > 0U) && (s[0] == ':')) {
        i = 1U;                                  /* leading ':' = root */
    }
    while ((i < len) && (n < SCPI_MAX_NODES)) {
        node_t *nd = &nodes[n];
        nd->optional   = false;
        nd->has_suffix = false;
        nd->suffix     = 1;
        if (is_pattern && (s[i] == '[')) {
            nd->optional = true;
            i++;
            if ((i < len) && (s[i] == ':')) {
                i++;                             /* "[:LEVel]" */
            }
        }
        nd->word = &s[i];
        nd->len  = 0U;
        if ((i < len) && (s[i] == '*')) {
            nd->len++;                           /* common command: "*RST" */
            i++;
        }
        while ((i < len) && chr_is_alpha(s[i])) {
            nd->len++;
            i++;
        }
        if (is_pattern && (i < len) && (s[i] == '#')) {
            nd->has_suffix = true;
            i++;
        } else if (!is_pattern && (i < len) && chr_is_digit(s[i])) {
            nd->has_suffix = true;
            nd->suffix     = 0;
            while ((i < len) && chr_is_digit(s[i])) {
                nd->suffix = (nd->suffix * 10) + (s[i] - '0');
                i++;
            }
        } else {
            /* no suffix */
        }
        if (is_pattern && (i < len) && (s[i] == ']')) {
            i++;
        }
        if ((i < len) && (s[i] == ':')) {
            i++;
        } else if (is_pattern && (i < len) && (s[i] == '[')) {
            /* "VOLTage[:DC]": the optional node starts right here */
        } else if (i < len) {
            return 0U;                           /* illegal character */
        } else {
            /* end */
        }
        if (nd->len == 0U) {
            return 0U;
        }
        n++;
    }
    return n;
}

/* Header word vs pattern node: short form (capitals) or long form. */
static bool word_match(const node_t *p, const node_t *h)
{
    uint32_t short_len = 0U;
    while ((short_len < p->len) && !chr_is_lower(p->word[short_len])) {
        short_len++;
    }
    if ((h->len != short_len) && (h->len != p->len)) {
        return false;                            /* partial forms are illegal */
    }
    for (uint32_t i = 0U; i < h->len; i++) {
        if (chr_to_upper(h->word[i]) != chr_to_upper(p->word[i])) {
            return false;
        }
    }
    return true;
}

typedef struct {
    const node_t *p;
    uint32_t      pn;
    const node_t *h;
    uint32_t      hn;
    int32_t      *suffix;
    uint32_t     *nsuffix;
} match_ctx_t;

static bool match_from(const match_ctx_t *c, uint32_t pi, uint32_t hi)
{
    if (pi == c->pn) {
        return hi == c->hn;
    }
    const node_t  *p     = &c->p[pi];
    const uint32_t saved = *c->nsuffix;

    if ((hi < c->hn) && word_match(p, &c->h[hi]) && (!c->h[hi].has_suffix || p->has_suffix)) {
        if (p->has_suffix && (*c->nsuffix < SCPI_MAX_SUFFIXES)) {
            c->suffix[(*c->nsuffix)++] = c->h[hi].suffix;   /* default 1 */
        }
        if (match_from(c, pi + 1U, hi + 1U)) {
            return true;
        }
        *c->nsuffix = saved;                     /* backtrack */
    }
    if (p->optional) {
        if (p->has_suffix && (*c->nsuffix < SCPI_MAX_SUFFIXES)) {
            c->suffix[(*c->nsuffix)++] = 1;
        }
        if (match_from(c, pi + 1U, hi)) {
            return true;
        }
        *c->nsuffix = saved;
    }
    return false;
}

bool scpi_match(const char *pattern, const char *header, size_t header_len,
                int32_t *suffix, uint32_t *nsuffix)
{
    node_t pn[SCPI_MAX_NODES];
    node_t hn[SCPI_MAX_NODES];
    bool   pq;
    bool   hq;
    const uint32_t np = split_nodes(pattern, str_len(pattern), true, pn, &pq);
    const uint32_t nh = split_nodes(header, header_len, false, hn, &hq);

    *nsuffix = 0U;
    if ((np == 0U) || (nh == 0U) || (pq != hq)) {
        return false;
    }
    const match_ctx_t c = { pn, np, hn, nh, suffix, nsuffix };
    return match_from(&c, 0U, 0U);
}

/* ------------------------------------------------------------------ */
/* Errors                                                              */
/* ------------------------------------------------------------------ */

void scpi_error_push(scpi_t *s, int32_t code)
{
    if (code == SCPI_ERR_NONE) {
        return;
    }
    s->esr |= 0x20U;                             /* command error bit */
    if (s->err_count >= SCPI_ERROR_QUEUE) {
        /* the last slot reports the overflow */
        s->errors[(s->err_head + SCPI_ERROR_QUEUE - 1U) % SCPI_ERROR_QUEUE] = (int16_t)SCPI_ERR_QUEUE_OVERFLOW;
        return;
    }
    s->errors[(s->err_head + s->err_count) % SCPI_ERROR_QUEUE] = (int16_t)code;
    s->err_count++;
}

int32_t scpi_error_pop(scpi_t *s)
{
    if (s->err_count == 0U) {
        return SCPI_ERR_NONE;
    }
    const int32_t e = s->errors[s->err_head];
    s->err_head = (uint8_t)((s->err_head + 1U) % SCPI_ERROR_QUEUE);
    s->err_count--;
    return e;
}

const char *scpi_error_text(int32_t code)
{
    switch (code) {
    case SCPI_ERR_NONE:              return "No error";
    case SCPI_ERR_SYNTAX:            return "Syntax error";
    case SCPI_ERR_INVALID_SEP:       return "Invalid separator";
    case SCPI_ERR_DATA_TYPE:         return "Data type error";
    case SCPI_ERR_PARAM_NOT_ALLOWED: return "Parameter not allowed";
    case SCPI_ERR_MISSING_PARAM:     return "Missing parameter";
    case SCPI_ERR_UNDEFINED_HEADER:  return "Undefined header";
    case SCPI_ERR_SUFFIX_RANGE:      return "Header suffix out of range";
    case SCPI_ERR_DATA_OUT_OF_RANGE: return "Data out of range";
    case SCPI_ERR_EXECUTION:         return "Execution error";
    case SCPI_ERR_QUEUE_OVERFLOW:    return "Queue overflow";
    default:                         return "Unknown error";
    }
}

/* ------------------------------------------------------------------ */
/* Results                                                             */
/* ------------------------------------------------------------------ */

static void out(scpi_t *s, const char *text)
{
    if (s->write != NULL) {
        s->write(text, str_len(text), s->ctx);
    }
}

void scpi_result_text(scpi_t *s, const char *text)
{
    if (!s->first_output) {
        out(s, ";");                     /* answers of one line are joined */
    }
    s->first_output = false;
    out(s, text);
}

void scpi_result_int(scpi_t *s, int32_t v)
{
    char buf[16];
    (void)fmt_snprintf(buf, sizeof(buf), "%ld", (long)v);
    scpi_result_text(s, buf);
}

void scpi_result_fixed(scpi_t *s, int32_t raw, uint32_t decimals)
{
    char     buf[24];
    uint32_t scale = 1U;
    for (uint32_t i = 0U; i < decimals; i++) {
        scale *= 10U;
    }
    (void)fmt_fixed(buf, sizeof(buf), raw, scale, decimals);
    scpi_result_text(s, buf);
}

void scpi_result_bool(scpi_t *s, bool v)
{
    scpi_result_text(s, v ? "1" : "0");
}

/* ------------------------------------------------------------------ */
/* Parameters                                                          */
/* ------------------------------------------------------------------ */

/* signed decimal integer at *p, advances *p */
static bool parse_exponent(const char **p, int32_t *v)
{
    bool    negative = false;
    int32_t e        = 0;
    bool    digits   = false;
    if ((**p == '+') || (**p == '-')) {
        negative = (**p == '-');
        (*p)++;
    }
    while (chr_is_digit(**p)) {
        if (e < 100) {
            e = (e * 10) + (**p - '0');
        }
        (*p)++;
        digits = true;
    }
    *v = negative ? -e : e;
    return digits;
}

static void skip_ws(scpi_args_t *a)
{
    while (chr_is_space(*a->text)) {
        a->text++;
    }
}

/* Take the next comma-separated parameter into buf. */
static bool next_param(scpi_args_t *a, char *buf, size_t size)
{
    size_t n = 0U;
    skip_ws(a);
    if (*a->text == '\0') {
        return false;
    }
    while ((*a->text != '\0') && (*a->text != ',')) {
        if (n < (size - 1U)) {
            buf[n++] = *a->text;
        }
        a->text++;
    }
    if (*a->text == ',') {
        a->text++;
    }
    while ((n > 0U) && chr_is_space(buf[n - 1U])) {
        n--;
    }
    buf[n] = '\0';
    return true;
}

bool scpi_args_empty(const scpi_args_t *a)
{
    const char *p = a->text;
    while (chr_is_space(*p)) {
        p++;
    }
    return *p == '\0';
}

/* SI prefix exponent of a unit string like "mV", "kHz", "MHZ", "uA" */
static bool unit_exponent(const char *u, int32_t *exp10)
{
    static const char *const units[] = { "V", "A", "HZ", "S", "OHM", "PCT", "C", "DEG", "W" };
    char up[12];
    (void)str_copy(up, u, sizeof(up));
    (void)str_to_upper(up);
    const size_t len = str_len(up);

    if (len == 0U) {
        *exp10 = 0;
        return true;
    }
    for (size_t k = 0U; k < (sizeof(units) / sizeof(units[0])); k++) {
        const size_t ul = str_len(units[k]);
        if ((len >= ul) && str_eq(&up[len - ul], units[k])) {
            const size_t plen = len - ul;
            const char  *pre  = up;
            if (plen == 0U)                              { *exp10 = 0;   return true; }
            if ((plen == 2U) && (str_ncmp(pre, "MA", 2U) == 0)) { *exp10 = 6; return true; }
            if (plen != 1U)                              { return false; }
            switch (pre[0]) {
            /* SCPI: "M" is milli, except MHZ and MOHM which are mega */
            case 'M': *exp10 = (str_eq(units[k], "HZ") || str_eq(units[k], "OHM")) ? 6 : -3; return true;
            case 'K': *exp10 = 3;  return true;
            case 'G': *exp10 = 9;  return true;
            case 'U': *exp10 = -6; return true;
            case 'N': *exp10 = -9; return true;
            case 'P': *exp10 = -12; return true;
            default:  return false;
            }
        }
    }
    return false;
}

bool scpi_param_fixed(scpi_args_t *a, uint32_t decimals, int32_t lo, int32_t hi,
                      int32_t def, int32_t *out_value)
{
    char        buf[32];
    const char *p;
    int64_t     mant     = 0;
    int32_t     frac     = 0;
    int32_t     exp10    = 0;
    bool        negative = false;
    bool        digits   = false;

    if (!next_param(a, buf, sizeof(buf))) {
        return false;
    }
    if (str_ieq(buf, "MIN") || str_ieq(buf, "MINimum")) { *out_value = lo;  return true; }
    if (str_ieq(buf, "MAX") || str_ieq(buf, "MAXimum")) { *out_value = hi;  return true; }
    if (str_ieq(buf, "DEF") || str_ieq(buf, "DEFault")) { *out_value = def; return true; }
    p = buf;
    if ((*p == '+') || (*p == '-')) {
        negative = (*p == '-');
        p++;
    }
    for (; chr_is_digit(*p); p++) {
        if (mant < 100000000000LL) {
            mant = (mant * 10) + (*p - '0');
        } else {
            exp10++;                             /* drop extra precision */
        }
        digits = true;
    }
    if (*p == '.') {
        p++;
        for (; chr_is_digit(*p); p++) {
            if (mant < 100000000000LL) {
                mant = (mant * 10) + (*p - '0');
                frac++;
            }
            digits = true;
        }
    }
    if (!digits) {
        return false;
    }
    if ((*p == 'e') || (*p == 'E')) {
        int32_t e = 0;
        p++;
        if (!parse_exponent(&p, &e)) {
            return false;
        }
        exp10 += e;
    }
    while (chr_is_space(*p)) {
        p++;
    }
    int32_t unit_exp;
    if (!unit_exponent(p, &unit_exp)) {
        return false;
    }
    /* value = mant * 10^(exp10 + unit_exp - frac); scaled by 10^decimals */
    int32_t shift = exp10 + unit_exp - frac + (int32_t)decimals;
    int64_t v     = mant;
    for (; (shift > 0) && (v < 100000000000000LL); shift--) {
        v *= 10;
    }
    if (shift > 0) {
        return false;                            /* overflow */
    }
    for (; shift < 0; shift++) {
        v = (shift == -1) ? ((v + 5) / 10) : (v / 10);   /* round last step */
    }
    if (negative) {
        v = -v;
    }
    if ((v < lo) || (v > hi)) {
        *out_value = (v < lo) ? lo : hi;
        return false;
    }
    *out_value = (int32_t)v;
    return true;
}

bool scpi_param_bool(scpi_args_t *a, bool *out_value)
{
    char buf[8];
    return next_param(a, buf, sizeof(buf)) && str_to_bool(buf, out_value);
}

bool scpi_param_string(scpi_args_t *a, char *buf, size_t size)
{
    if (!next_param(a, buf, size)) {
        return false;
    }
    const size_t n = str_len(buf);
    if ((n >= 2U) && ((buf[0] == '"') || (buf[0] == '\'')) && (buf[n - 1U] == buf[0])) {
        for (size_t i = 0U; i < (n - 2U); i++) {
            buf[i] = buf[i + 1U];
        }
        buf[n - 2U] = '\0';
    }
    return true;
}

int32_t scpi_param_choice(scpi_args_t *a, const char *const *choices, uint32_t n)
{
    char buf[24];
    if (!next_param(a, buf, sizeof(buf))) {
        return -1;
    }
    for (uint32_t i = 0U; i < n; i++) {
        int32_t  sfx[SCPI_MAX_SUFFIXES];
        uint32_t ns;
        if (scpi_match(choices[i], buf, str_len(buf), sfx, &ns)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Execution                                                           */
/* ------------------------------------------------------------------ */

static int32_t common_command(scpi_t *s, const char *hdr, size_t len, scpi_args_t *args)
{
    char h[8];
    if (len >= sizeof(h)) {
        return SCPI_ERR_UNDEFINED_HEADER;
    }
    for (size_t i = 0U; i < len; i++) {
        h[i] = chr_to_upper(hdr[i]);
    }
    h[len] = '\0';
    if (str_eq(h, "*IDN?")) {
        scpi_result_text(s, s->idn);
        return SCPI_ERR_NONE;
    }
    if (str_eq(h, "*CLS")) {
        s->err_count = 0U;
        s->esr       = 0U;
        return SCPI_ERR_NONE;
    }
    if (str_eq(h, "*OPC?")) {
        scpi_result_text(s, "1");          /* commands complete synchronously */
        return SCPI_ERR_NONE;
    }
    if (str_eq(h, "*OPC") || str_eq(h, "*WAI")) {
        return SCPI_ERR_NONE;
    }
    if (str_eq(h, "*ESR?")) {
        scpi_result_int(s, s->esr);
        s->esr = 0U;
        return SCPI_ERR_NONE;
    }
    (void)args;
    return SCPI_ERR_UNDEFINED_HEADER;      /* *RST and *TST? go in the table */
}

static void execute_one(scpi_t *s, const char *cmd)
{
    while (chr_is_space(*cmd)) {
        cmd++;
    }
    if (*cmd == '\0') {
        return;
    }
    /* header = up to the first space */
    size_t hlen = 0U;
    while ((cmd[hlen] != '\0') && !chr_is_space(cmd[hlen])) {
        hlen++;
    }
    scpi_args_t args;
    args.text    = &cmd[hlen];
    args.nsuffix = 0U;

    /* the application table first (it may override *RST, *TST?...) */
    for (uint32_t i = 0U; i < s->count; i++) {
        if (scpi_match(s->commands[i].pattern, cmd, hlen, args.suffix, &args.nsuffix)) {
            scpi_error_push(s, s->commands[i].handler(s, &args));
            return;
        }
    }
    if (cmd[0] == '*') {
        scpi_error_push(s, common_command(s, cmd, hlen, &args));
        return;
    }
    /* mandatory SYSTem subsystem (SCPI-99 chapter 4) */
    if (scpi_match("SYSTem:ERRor[:NEXT]?", cmd, hlen, args.suffix, &args.nsuffix)) {
        char          buf[48];
        const int32_t e = scpi_error_pop(s);
        (void)fmt_snprintf(buf, sizeof(buf), "%ld,\"%s\"", (long)e, scpi_error_text(e));
        scpi_result_text(s, buf);
        return;
    }
    if (scpi_match("SYSTem:ERRor:COUNt?", cmd, hlen, args.suffix, &args.nsuffix)) {
        scpi_result_int(s, s->err_count);
        return;
    }
    if (scpi_match("SYSTem:VERSion?", cmd, hlen, args.suffix, &args.nsuffix)) {
        scpi_result_text(s, "1999.0");
        return;
    }
    scpi_error_push(s, SCPI_ERR_UNDEFINED_HEADER);
}

void scpi_execute(scpi_t *s, const char *line)
{
    char  buf[SCPI_INPUT_MAX];
    char *start = buf;
    bool  quote = false;

    (void)str_copy(buf, line, sizeof(buf));
    s->first_output = true;
    for (char *p = buf; ; p++) {
        if (*p == '"') {
            quote = !quote;
        }
        if (((*p == ';') && !quote) || (*p == '\0')) {
            const bool last = (*p == '\0');
            *p = '\0';
            execute_one(s, start);
            if (last) {
                break;
            }
            start = p + 1;
        }
    }
    if (!s->first_output) {
        out(s, "\n");                      /* response terminator */
    }
}

void scpi_input(scpi_t *s, const char *data, size_t len)
{
    for (size_t i = 0U; i < len; i++) {
        const char c = data[i];
        if ((c == '\n') || (c == '\r')) {
            if (s->len > 0U) {
                s->line[s->len] = '\0';
                scpi_execute(s, s->line);
                s->len = 0U;
            }
        } else if (s->len < (SCPI_INPUT_MAX - 1U)) {
            s->line[s->len++] = c;
        } else {
            /* line too long: drop it and report */
            s->len = 0U;
            scpi_error_push(s, SCPI_ERR_SYNTAX);
        }
    }
}

void scpi_init(scpi_t *s, const scpi_command_t *commands, uint32_t count, const char *idn,
               scpi_write_fn write, void *ctx)
{
    s->commands     = commands;
    s->count        = count;
    s->idn          = idn;
    s->write        = write;
    s->ctx          = ctx;
    s->len          = 0U;
    s->err_head     = 0U;
    s->err_count    = 0U;
    s->esr          = 0U;
    s->first_output = true;
}

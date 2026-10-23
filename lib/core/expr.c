/**
 * @file    expr.c
 * @brief   Recursive descent expression evaluator (see expr.h).
 *
 * One function per precedence level; each calls the next tighter
 * level. Arithmetic is done in uint32_t to get defined wrap-around
 * (signed overflow is undefined behaviour in C), then cast back.
 */
#include "expr.h"
#include "strutil.h"

#include <stddef.h>

#define EXPR_MAX_DEPTH  (32U)   /* recursion guard: small MCU stacks */

typedef struct {
    const char   *start;
    const char   *p;
    expr_var_fn   vars;
    expr_status_t err;
    uint32_t      depth;
} parser_t;

static uint32_t s_error_pos;

static int32_t parse_or(parser_t *ps);

static void skip_spaces(parser_t *ps)
{
    while (chr_is_space(*ps->p)) {
        ps->p++;
    }
}

static void fail(parser_t *ps, expr_status_t err)
{
    if (ps->err == EXPR_OK) {
        ps->err     = err;
        s_error_pos = (uint32_t)(ps->p - ps->start);
    }
}

/* Match a 1 or 2 character operator; consumes it on success. */
static bool accept(parser_t *ps, const char *op)
{
    skip_spaces(ps);
    const size_t n = str_len(op);
    if (str_ncmp(ps->p, op, n) != 0) {
        return false;
    }
    /* do not take '<' when the text is '<<' or '<=', etc. */
    if (n == 1U) {
        const char next = ps->p[1];
        if (((op[0] == '<') || (op[0] == '>')) && ((next == op[0]) || (next == '='))) {
            return false;
        }
        if (((op[0] == '&') || (op[0] == '|')) && (next == op[0])) {
            return false;
        }
        if ((op[0] == '!') && (next == '=')) {
            return false;
        }
    }
    ps->p += n;
    return true;
}

static int32_t parse_number(parser_t *ps)
{
    char     buf[40];
    uint32_t n = 0U;
    uint32_t v;

    if ((ps->p[0] == '\'') && (ps->p[1] != '\0') && (ps->p[2] == '\'')) {
        v = (uint8_t)ps->p[1];
        ps->p += 3;
        return (int32_t)v;
    }
    while ((chr_is_alnum(*ps->p) || (*ps->p == '_')) && (n < (sizeof(buf) - 1U))) {
        buf[n++] = *ps->p++;
    }
    buf[n] = '\0';
    if (!str_to_u32(buf, &v)) {
        fail(ps, EXPR_ERR_SYNTAX);
        return 0;
    }
    return (int32_t)v;
}

static int32_t parse_primary(parser_t *ps)
{
    skip_spaces(ps);
    if (ps->depth++ > EXPR_MAX_DEPTH) {
        fail(ps, EXPR_ERR_DEPTH);
        return 0;
    }
    int32_t v = 0;
    if (accept(ps, "(")) {
        v = parse_or(ps);
        if (!accept(ps, ")")) {
            fail(ps, EXPR_ERR_PAREN);
        }
    } else if (accept(ps, "-")) {
        v = (int32_t)(0U - (uint32_t)parse_primary(ps));
    } else if (accept(ps, "+")) {
        v = parse_primary(ps);
    } else if (accept(ps, "~")) {
        v = (int32_t)~(uint32_t)parse_primary(ps);
    } else if (accept(ps, "!")) {
        v = (parse_primary(ps) == 0) ? 1 : 0;
    } else if (chr_is_digit(*ps->p) || (*ps->p == '\'')) {
        v = parse_number(ps);
    } else if (chr_is_alpha(*ps->p) || (*ps->p == '_')) {
        const char *name = ps->p;
        while (chr_is_alnum(*ps->p) || (*ps->p == '_')) {
            ps->p++;
        }
        if ((ps->vars == NULL) || !ps->vars(name, (uint32_t)(ps->p - name), &v)) {
            ps->p = name;
            fail(ps, EXPR_ERR_SYNTAX);
        }
    } else {
        fail(ps, EXPR_ERR_SYNTAX);
    }
    ps->depth--;
    return v;
}

static int32_t parse_mul(parser_t *ps)
{
    int32_t v = parse_primary(ps);
    for (;;) {
        if (accept(ps, "*")) {
            v = (int32_t)((uint32_t)v * (uint32_t)parse_primary(ps));
        } else if (accept(ps, "/") || accept(ps, "%")) {
            const bool    is_div = (ps->p[-1] == '/');
            const int32_t r      = parse_primary(ps);
            if (r == 0) {
                fail(ps, EXPR_ERR_DIV_ZERO);
                return 0;
            }
            if ((v == INT32_MIN) && (r == -1)) {
                v = is_div ? INT32_MIN : 0;      /* would overflow in C */
            } else {
                v = is_div ? (v / r) : (v % r);
            }
        } else {
            return v;
        }
    }
}

static int32_t parse_add(parser_t *ps)
{
    int32_t v = parse_mul(ps);
    for (;;) {
        if (accept(ps, "+")) {
            v = (int32_t)((uint32_t)v + (uint32_t)parse_mul(ps));
        } else if (accept(ps, "-")) {
            v = (int32_t)((uint32_t)v - (uint32_t)parse_mul(ps));
        } else {
            return v;
        }
    }
}

static int32_t parse_shift(parser_t *ps)
{
    int32_t v = parse_add(ps);
    for (;;) {
        if (accept(ps, "<<")) {
            const uint32_t n = (uint32_t)parse_add(ps) & 31U;
            v = (int32_t)((uint32_t)v << n);
        } else if (accept(ps, ">>")) {
            /* logical shift: registers are unsigned */
            const uint32_t n = (uint32_t)parse_add(ps) & 31U;
            v = (int32_t)((uint32_t)v >> n);
        } else {
            return v;
        }
    }
}

static int32_t parse_rel(parser_t *ps)
{
    int32_t v = parse_shift(ps);
    for (;;) {
        if (accept(ps, "<="))     { v = (v <= parse_shift(ps)) ? 1 : 0; }
        else if (accept(ps, ">=")) { v = (v >= parse_shift(ps)) ? 1 : 0; }
        else if (accept(ps, "<"))  { v = (v <  parse_shift(ps)) ? 1 : 0; }
        else if (accept(ps, ">"))  { v = (v >  parse_shift(ps)) ? 1 : 0; }
        else                       { return v; }
    }
}

static int32_t parse_eq(parser_t *ps)
{
    int32_t v = parse_rel(ps);
    for (;;) {
        if (accept(ps, "=="))      { v = (v == parse_rel(ps)) ? 1 : 0; }
        else if (accept(ps, "!=")) { v = (v != parse_rel(ps)) ? 1 : 0; }
        else                       { return v; }
    }
}

static int32_t parse_band(parser_t *ps)
{
    int32_t v = parse_eq(ps);
    while (accept(ps, "&")) {
        v = (int32_t)((uint32_t)v & (uint32_t)parse_eq(ps));
    }
    return v;
}

static int32_t parse_xor(parser_t *ps)
{
    int32_t v = parse_band(ps);
    while (accept(ps, "^")) {
        v = (int32_t)((uint32_t)v ^ (uint32_t)parse_band(ps));
    }
    return v;
}

static int32_t parse_bor(parser_t *ps)
{
    int32_t v = parse_xor(ps);
    while (accept(ps, "|")) {
        v = (int32_t)((uint32_t)v | (uint32_t)parse_xor(ps));
    }
    return v;
}

static int32_t parse_and(parser_t *ps)
{
    int32_t v = parse_bor(ps);
    while (accept(ps, "&&")) {
        const int32_t r = parse_bor(ps);     /* both sides evaluated: no side effects */
        v = ((v != 0) && (r != 0)) ? 1 : 0;
    }
    return v;
}

static int32_t parse_or(parser_t *ps)
{
    int32_t v = parse_and(ps);
    while (accept(ps, "||")) {
        const int32_t r = parse_and(ps);
        v = ((v != 0) || (r != 0)) ? 1 : 0;
    }
    return v;
}

expr_status_t expr_eval(const char *text, int32_t *result, expr_var_fn vars)
{
    parser_t ps = { text, text, vars, EXPR_OK, 0U };

    s_error_pos = 0U;
    const int32_t v = parse_or(&ps);
    skip_spaces(&ps);
    if ((ps.err == EXPR_OK) && (*ps.p != '\0')) {
        fail(&ps, (*ps.p == ')') ? EXPR_ERR_PAREN : EXPR_ERR_SYNTAX);
    }
    if (ps.err == EXPR_OK) {
        *result = v;
    }
    return ps.err;
}

uint32_t expr_error_pos(void)
{
    return s_error_pos;
}

const char *expr_status_str(expr_status_t st)
{
    switch (st) {
    case EXPR_OK:           return "ok";
    case EXPR_ERR_SYNTAX:   return "syntax error";
    case EXPR_ERR_DIV_ZERO: return "division by zero";
    case EXPR_ERR_PAREN:    return "unbalanced parenthesis";
    case EXPR_ERR_DEPTH:    return "expression too deep";
    default:                return "?";
    }
}

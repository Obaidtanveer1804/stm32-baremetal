/**
 * @file    expr.h
 * @brief   Integer expression evaluator (recursive descent, C rules).
 *
 * The CLI "calc" command uses it to do register arithmetic on the
 * board itself:
 *
 *   calc (1<<8)|(1<<9)          -> 768  0x300
 *   calc 0x4001100C + 4*3
 *   calc ~0xFF & 0xFFFF
 *   calc 72000000 / (71+1) / (999+1)
 *
 * Precedence (high to low), same as C:
 *   ( )   unary - ~ !   * / %   + -   << >>   < <= > >=   == !=
 *   &   ^   |   &&   ||
 * Numbers: decimal, 0x hex, 0b binary, 0o octal, '_' separators,
 * 'c' character literals. 32-bit signed arithmetic, wrap on overflow,
 * division by zero is an error.
 */
#ifndef EXPR_H
#define EXPR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EXPR_OK = 0,
    EXPR_ERR_SYNTAX,
    EXPR_ERR_DIV_ZERO,
    EXPR_ERR_PAREN,
    EXPR_ERR_DEPTH
} expr_status_t;

/** Optional variable lookup: return true and *value if @p name exists. */
typedef bool (*expr_var_fn)(const char *name, uint32_t len, int32_t *value);

expr_status_t expr_eval(const char *text, int32_t *result, expr_var_fn vars);
/** Column (0-based) where the last error was detected. */
uint32_t      expr_error_pos(void);
const char   *expr_status_str(expr_status_t st);

#endif /* EXPR_H */

/**
 * @file    scpi.h
 * @brief   SCPI (IEEE 488.2) command parser: turns the board into a
 *          lab instrument that PyVISA / NI-VISA / a terminal can drive,
 *          exactly like the oscilloscopes and power supplies of the
 *          instrumentation TPs.
 *
 *   *IDN?                          -> "ELISE,STM32-INSTR,<serial>,1.0"
 *   MEASure:VOLTage? 1             -> "1.6503"
 *   meas:volt? (CH1)               (short form, any case)
 *   SOURce1:VOLTage 1.25           (numeric suffix = channel)
 *   OUTPut:STATe ON ; *OPC?        (several commands per line)
 *   SYSTem:ERRor?                  -> "-113,\"Undefined header\""
 *
 * Patterns: UPPER part = short form, lower part = rest of the long
 * form, [ ] = optional node, # = numeric suffix (default 1), '?' at the
 * end for queries. Example: "SOURce#:VOLTage[:LEVel]".
 */
#ifndef SCPI_H
#define SCPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SCPI_INPUT_MAX     (128U)
#define SCPI_ERROR_QUEUE   (8U)
#define SCPI_MAX_SUFFIXES  (3U)

/* Standard error codes (SCPI-99 chapter 21) */
#define SCPI_ERR_NONE              (0)
#define SCPI_ERR_SYNTAX            (-102)
#define SCPI_ERR_INVALID_SEP       (-103)
#define SCPI_ERR_DATA_TYPE         (-104)
#define SCPI_ERR_PARAM_NOT_ALLOWED (-108)
#define SCPI_ERR_MISSING_PARAM     (-109)
#define SCPI_ERR_UNDEFINED_HEADER  (-113)
#define SCPI_ERR_SUFFIX_RANGE      (-114)
#define SCPI_ERR_DATA_OUT_OF_RANGE (-222)
#define SCPI_ERR_EXECUTION         (-200)
#define SCPI_ERR_QUEUE_OVERFLOW    (-350)

struct scpi;

typedef struct {
    const char *text;          /**< remaining parameter text            */
    int32_t     suffix[SCPI_MAX_SUFFIXES];  /**< numeric suffixes in order */
    uint32_t    nsuffix;
} scpi_args_t;

/** Return SCPI_ERR_NONE or an error code (pushed to the queue). */
typedef int32_t (*scpi_handler_t)(struct scpi *s, scpi_args_t *args);

typedef struct {
    const char    *pattern;
    scpi_handler_t handler;
} scpi_command_t;

typedef void (*scpi_write_fn)(const char *text, size_t len, void *ctx);

typedef struct scpi {
    const scpi_command_t *commands;
    uint32_t              count;
    scpi_write_fn         write;
    void                 *ctx;
    const char           *idn;            /**< "Maker,Model,Serial,Version" */
    char                  line[SCPI_INPUT_MAX];
    uint32_t              len;
    int16_t               errors[SCPI_ERROR_QUEUE];
    uint8_t               err_head;
    uint8_t               err_count;
    uint8_t               esr;            /**< standard event status register */
    bool                  first_output;   /**< ';' between query answers */
} scpi_t;

void    scpi_init(scpi_t *s, const scpi_command_t *commands, uint32_t count, const char *idn,
                  scpi_write_fn write, void *ctx);
/** Feed received characters; executes on '\n'. */
void    scpi_input(scpi_t *s, const char *data, size_t len);
/** Execute one full line ("CMD1 x;CMD2?"). */
void    scpi_execute(scpi_t *s, const char *line);
void    scpi_error_push(scpi_t *s, int32_t code);
int32_t scpi_error_pop(scpi_t *s);
const char *scpi_error_text(int32_t code);

/* ------------------- helpers for command handlers ----------------- */
/**
 * Parse a number with optional SI prefix + unit: "1.25", "250mV",
 * "2.5kHz", "-3e-3", "MIN"/"MAX"/"DEF" (returns lo / hi / def).
 * Result is scaled: value * 10^decimals (1.25 V, 3 -> 1250).
 */
bool    scpi_param_fixed(scpi_args_t *a, uint32_t decimals, int32_t lo, int32_t hi,
                         int32_t def, int32_t *out);
bool    scpi_param_bool(scpi_args_t *a, bool *out);
/** Quoted or bare word into buf. */
bool    scpi_param_string(scpi_args_t *a, char *buf, size_t size);
/** Match one of the choices (SCPI keywords like "SINusoid"). Returns index or -1. */
int32_t scpi_param_choice(scpi_args_t *a, const char *const *choices, uint32_t n);
bool    scpi_args_empty(const scpi_args_t *a);

void    scpi_result_text(scpi_t *s, const char *text);
void    scpi_result_int(scpi_t *s, int32_t v);
/** Fixed-point result: raw / 10^decimals, e.g. (16503, 4) -> "1.6503". */
void    scpi_result_fixed(scpi_t *s, int32_t raw, uint32_t decimals);
void    scpi_result_bool(scpi_t *s, bool v);

/** Pattern matcher (portable, unit-tested): fills the suffixes. */
bool    scpi_match(const char *pattern, const char *header, size_t header_len,
                   int32_t *suffix, uint32_t *nsuffix);

#endif /* SCPI_H */

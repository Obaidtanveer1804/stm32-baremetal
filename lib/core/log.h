/**
 * @file    log.h
 * @brief   Levelled logging with timestamps, module tags and colours.
 *
 *   LOG_I("adc", "ch%u = %u", ch, value);
 *   -> [   1234] I adc: ch3 = 2048
 *
 * Two filters:
 *  - compile time: LOG_COMPILE_LEVEL removes lower levels from flash
 *    (define it in the Makefile, e.g. -DLOG_COMPILE_LEVEL=LOG_LEVEL_WARN)
 *  - run time: log_set_level() (the CLI "log" command uses it)
 */
#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stdbool.h>

#define LOG_LEVEL_NONE   (0)
#define LOG_LEVEL_ERROR  (1)
#define LOG_LEVEL_WARN   (2)
#define LOG_LEVEL_INFO   (3)
#define LOG_LEVEL_DEBUG  (4)
#define LOG_LEVEL_TRACE  (5)

#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL  LOG_LEVEL_DEBUG
#endif

typedef uint32_t (*log_time_fn)(void);

void     log_init(log_time_fn time_ms);
void     log_set_level(int32_t level);
int32_t  log_get_level(void);
void     log_set_color(bool enable);
const char *log_level_name(int32_t level);
/** Parse "error/warn/info/debug/trace/none" or a digit. */
bool     log_parse_level(const char *s, int32_t *level);

void     log_write(int32_t level, const char *tag, const char *fmt, ...)
            __attribute__((format(printf, 3, 4)));

#if LOG_COMPILE_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E(tag, ...)  log_write(LOG_LEVEL_ERROR, (tag), __VA_ARGS__)
#else
#define LOG_E(tag, ...)  ((void)0)
#endif

#if LOG_COMPILE_LEVEL >= LOG_LEVEL_WARN
#define LOG_W(tag, ...)  log_write(LOG_LEVEL_WARN, (tag), __VA_ARGS__)
#else
#define LOG_W(tag, ...)  ((void)0)
#endif

#if LOG_COMPILE_LEVEL >= LOG_LEVEL_INFO
#define LOG_I(tag, ...)  log_write(LOG_LEVEL_INFO, (tag), __VA_ARGS__)
#else
#define LOG_I(tag, ...)  ((void)0)
#endif

#if LOG_COMPILE_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_D(tag, ...)  log_write(LOG_LEVEL_DEBUG, (tag), __VA_ARGS__)
#else
#define LOG_D(tag, ...)  ((void)0)
#endif

#if LOG_COMPILE_LEVEL >= LOG_LEVEL_TRACE
#define LOG_T(tag, ...)  log_write(LOG_LEVEL_TRACE, (tag), __VA_ARGS__)
#else
#define LOG_T(tag, ...)  ((void)0)
#endif

/* ------------------------------ assert ---------------------------- */
/**
 * ASSERT(cond): on failure prints file:line and calls the fault hook
 * (default: print and spin forever, so a debugger can inspect it).
 * Define NDEBUG to compile the checks out.
 */
typedef void (*assert_hook_fn)(const char *file, uint32_t line, const char *expr);
void assert_set_hook(assert_hook_fn hook);
void assert_failed(const char *file, uint32_t line, const char *expr);

#ifdef NDEBUG
#define ASSERT(cond)  ((void)0)
#else
#define ASSERT(cond) \
    do { if (!(cond)) { assert_failed(__FILE__, (uint32_t)__LINE__, #cond); } } while (0)
#endif

#endif /* LOG_H */

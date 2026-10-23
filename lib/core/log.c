/**
 * @file    log.c
 * @brief   Logging and assertions (see log.h).
 */
#include "log.h"
#include "fmt.h"
#include "io.h"
#include "strutil.h"

#include <stdarg.h>
#include <stddef.h>

static log_time_fn    s_time;
static int32_t        s_level = LOG_LEVEL_INFO;
static bool           s_color = true;
static assert_hook_fn s_assert_hook;

static const char *const level_names[] = {
    "none", "error", "warn", "info", "debug", "trace"
};
static const char level_letters[] = "-EWIDT";
static const uint8_t level_colors[] = {
    IO_COLOR_RESET, IO_COLOR_RED, IO_COLOR_YELLOW, IO_COLOR_GREEN,
    IO_COLOR_CYAN, IO_COLOR_MAGENTA
};

void log_init(log_time_fn time_ms)
{
    s_time = time_ms;
}

void log_set_level(int32_t level)
{
    if ((level >= LOG_LEVEL_NONE) && (level <= LOG_LEVEL_TRACE)) {
        s_level = level;
    }
}

int32_t log_get_level(void)
{
    return s_level;
}

void log_set_color(bool enable)
{
    s_color = enable;
}

const char *log_level_name(int32_t level)
{
    return ((level >= LOG_LEVEL_NONE) && (level <= LOG_LEVEL_TRACE)) ? level_names[level] : "?";
}

bool log_parse_level(const char *s, int32_t *level)
{
    for (int32_t i = LOG_LEVEL_NONE; i <= LOG_LEVEL_TRACE; i++) {
        if (str_ieq(s, level_names[i])) {
            *level = i;
            return true;
        }
    }
    int32_t v;
    if (str_to_i32(s, &v) && (v >= LOG_LEVEL_NONE) && (v <= LOG_LEVEL_TRACE)) {
        *level = v;
        return true;
    }
    return false;
}

static void log_sink(char ch, void *ctx)
{
    (void)ctx;
    io_putc(ch);
}

void log_write(int32_t level, const char *tag, const char *fmt, ...)
{
    va_list args;

    if ((level > s_level) || (level <= LOG_LEVEL_NONE) || (level > LOG_LEVEL_TRACE)) {
        return;
    }
    if (s_color) {
        io_ansi_color(level_colors[level]);
    }
    io_printf("[%7lu] %c %s: ",
              (unsigned long)((s_time != NULL) ? s_time() : 0U),
              level_letters[level], (tag != NULL) ? tag : "-");
    va_start(args, fmt);
    (void)fmt_vformat(log_sink, NULL, fmt, args);
    va_end(args);
    if (s_color) {
        io_ansi_color(IO_COLOR_RESET);
    }
    io_puts(IO_NEWLINE);
}

/* ------------------------------ assert ---------------------------- */

void assert_set_hook(assert_hook_fn hook)
{
    s_assert_hook = hook;
}

void assert_failed(const char *file, uint32_t line, const char *expr)
{
    io_printf(IO_NEWLINE "ASSERT FAILED %s:%lu: %s" IO_NEWLINE,
              file, (unsigned long)line, expr);
    if (s_assert_hook != NULL) {
        s_assert_hook(file, line, expr);
        return;
    }
    for (;;) {
        /* halt here: attach GDB and run "bt" to see who failed */
    }
}

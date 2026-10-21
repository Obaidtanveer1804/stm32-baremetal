/**
 * @file    cli.c
 * @brief   Shell engine: line editor, history, completion, dispatch.
 */
#include "cli.h"
#include "io.h"
#include "fmt.h"
#include "strutil.h"
#include "hal_time.h"

#include <stdarg.h>
#include <stddef.h>

/* ASCII control characters */
#define KEY_CTRL_A     (0x01)
#define KEY_CTRL_C     (0x03)
#define KEY_CTRL_D     (0x04)
#define KEY_CTRL_E     (0x05)
#define KEY_BACKSPACE  (0x08)
#define KEY_TAB        (0x09)
#define KEY_LF         (0x0A)
#define KEY_CTRL_K     (0x0B)
#define KEY_CTRL_L     (0x0C)
#define KEY_CR         (0x0D)
#define KEY_CTRL_N     (0x0E)
#define KEY_CTRL_P     (0x10)
#define KEY_CTRL_U     (0x15)
#define KEY_CTRL_W     (0x17)
#define KEY_ESC        (0x1B)
#define KEY_DEL        (0x7F)

typedef enum {
    ESC_NONE = 0,
    ESC_START,       /* got ESC                    */
    ESC_CSI,         /* got ESC [  (or ESC O)       */
} esc_state_t;

typedef struct {
    const char        *prompt;
    char               line[CLI_LINE_MAX];
    uint32_t           len;
    uint32_t           cursor;
    esc_state_t        esc;
    uint32_t           esc_param;
    char               last_char;
    bool               echo;

    char               history[CLI_HISTORY_DEPTH][CLI_LINE_MAX];
    uint32_t           hist_count;      /* entries stored (<= depth)   */
    uint32_t           hist_next;       /* slot written next           */
    int32_t            hist_browse;     /* -1: editing a new line      */

    const cli_group_t *groups[CLI_GROUPS_MAX];
    uint32_t           ngroups;
} cli_state_t;

static cli_state_t s_cli;

/* 4-byte aligned for int16/int32/struct buffers */
static uint32_t s_scratch[CLI_SCRATCH_SIZE / sizeof(uint32_t)];

void *cli_scratch(uint32_t size)
{
    return (size <= sizeof(s_scratch)) ? (void *)s_scratch : NULL;
}

/* ------------------------------------------------------------------ */
/* Output helpers                                                      */
/* ------------------------------------------------------------------ */

static void cursor_left(uint32_t n)
{
    if (n > 0U) {
        io_printf("\x1b[%luD", (unsigned long)n);
    }
}

static void cursor_right(uint32_t n)
{
    if (n > 0U) {
        io_printf("\x1b[%luC", (unsigned long)n);
    }
}

void cli_print_prompt(void)
{
    io_ansi_color(IO_COLOR_GREEN);
    io_puts(s_cli.prompt);
    io_ansi_color(IO_COLOR_RESET);
}

/* Redraw the whole line (after history recall, completion, Ctrl-L). */
static void redraw_line(void)
{
    io_ansi_clear_line();
    cli_print_prompt();
    io_write(s_cli.line, s_cli.len);
    cursor_left(s_cli.len - s_cli.cursor);
}

int cli_usage(const char *name, const char *usage)
{
    io_printf("usage: %s %s" IO_NEWLINE, name, (usage != NULL) ? usage : "");
    return CLI_ERR_USAGE;
}

static void cli_out_sink(char ch, void *ctx)
{
    (void)ctx;
    io_putc(ch);
}

int cli_error(const char *fmt, ...)
{
    va_list args;
    io_ansi_color(IO_COLOR_RED);
    io_puts("error: ");
    va_start(args, fmt);
    (void)fmt_vformat(cli_out_sink, NULL, fmt, args);
    va_end(args);
    io_ansi_color(IO_COLOR_RESET);
    io_puts(IO_NEWLINE);
    return CLI_ERR;
}

/* ------------------------------------------------------------------ */
/* Argument helpers                                                    */
/* ------------------------------------------------------------------ */

bool cli_arg_u32(const char *s, uint32_t *v)
{
    if (!str_to_u32(s, v)) {
        (void)cli_error("'%s' is not a number (use 123, 0x7B or 0b1111011)", s);
        return false;
    }
    return true;
}

bool cli_arg_i32(const char *s, int32_t *v)
{
    if (!str_to_i32(s, v)) {
        (void)cli_error("'%s' is not a signed number", s);
        return false;
    }
    return true;
}

bool cli_arg_u32_range(const char *s, uint32_t lo, uint32_t hi, uint32_t *v)
{
    if (!cli_arg_u32(s, v)) {
        return false;
    }
    if ((*v < lo) || (*v > hi)) {
        (void)cli_error("%lu out of range [%lu..%lu]", (unsigned long)*v,
                        (unsigned long)lo, (unsigned long)hi);
        return false;
    }
    return true;
}

bool cli_arg_bool(const char *s, bool *v)
{
    if (!str_to_bool(s, v)) {
        (void)cli_error("'%s' is not on/off/1/0", s);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Command lookup                                                      */
/* ------------------------------------------------------------------ */

const cli_command_t *cli_find(const char *name, bool *ambiguous)
{
    const cli_command_t *prefix_match = NULL;
    uint32_t             prefix_count = 0U;
    const size_t         len          = str_len(name);

    *ambiguous = false;
    for (uint32_t g = 0U; g < s_cli.ngroups; g++) {
        const cli_group_t *grp = s_cli.groups[g];
        for (uint32_t i = 0U; i < grp->count; i++) {
            const cli_command_t *c = &grp->commands[i];
            if (str_eq(c->name, name)) {
                return c;                        /* exact match wins */
            }
            if ((len > 0U) && (str_ncmp(c->name, name, len) == 0)) {
                prefix_match = c;
                prefix_count++;
            }
        }
    }
    if (prefix_count == 1U) {
        return prefix_match;
    }
    *ambiguous = prefix_count > 1U;
    return NULL;
}

static void list_prefix_matches(const char *name)
{
    const size_t len = str_len(name);
    for (uint32_t g = 0U; g < s_cli.ngroups; g++) {
        const cli_group_t *grp = s_cli.groups[g];
        for (uint32_t i = 0U; i < grp->count; i++) {
            if (str_ncmp(grp->commands[i].name, name, len) == 0) {
                io_printf("%s  ", grp->commands[i].name);
            }
        }
    }
    io_puts(IO_NEWLINE);
}

#define CLI_HELP_COLUMN  (32U)

/* "usage   description"; a long usage gets the description on its own line */
static void print_help_line(const char *left, const char *help)
{
    const size_t len = str_len(left);
    io_printf("  %s", left);
    if (len >= CLI_HELP_COLUMN) {
        io_puts(IO_NEWLINE);
        io_repeat(' ', CLI_HELP_COLUMN + 3U);
    } else {
        io_repeat(' ', (uint32_t)(CLI_HELP_COLUMN + 1U - len));
    }
    io_printf("%s" IO_NEWLINE, (help != NULL) ? help : "");
}

static const cli_subcmd_t *find_sub(const cli_subcmd_t *subs, const char *name)
{
    const cli_subcmd_t *match = NULL;
    uint32_t            count = 0U;
    const size_t        len   = str_len(name);

    for (const cli_subcmd_t *s = subs; s->name != NULL; s++) {
        if (str_eq(s->name, name)) {
            return s;
        }
        if (str_ncmp(s->name, name, len) == 0) {
            match = s;
            count++;
        }
    }
    return (count == 1U) ? match : NULL;
}

static void print_subcmds(const cli_subcmd_t *subs, const char *parent)
{
    for (const cli_subcmd_t *s = subs; s->name != NULL; s++) {
        char left[CLI_LINE_MAX];
        (void)fmt_snprintf(left, sizeof(left), "%s %s %s", parent, s->name,
                           (s->usage != NULL) ? s->usage : "");
        print_help_line(left, s->help);
    }
}

int cli_dispatch_subcmd(const cli_subcmd_t *subs, int argc, char *argv[], const char *parent)
{
    if (argc < 2) {
        io_printf("%s: sub-command needed" IO_NEWLINE, parent);
        print_subcmds(subs, parent);
        return CLI_ERR_USAGE + 100;    /* usage already printed */
    }
    const cli_subcmd_t *s = find_sub(subs, argv[1]);
    if (s == NULL) {
        (void)cli_error("%s: unknown sub-command '%s'", parent, argv[1]);
        print_subcmds(subs, parent);
        return CLI_ERR;
    }
    const int rc = s->handler(argc - 1, &argv[1]);
    if (rc == CLI_ERR_USAGE) {
        io_printf("usage: %s %s %s" IO_NEWLINE, parent, s->name, (s->usage != NULL) ? s->usage : "");
        return CLI_ERR;
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* Execution                                                           */
/* ------------------------------------------------------------------ */

static int exec_argv(int argc, char *argv[])
{
    bool ambiguous;

    if (argc == 0) {
        return CLI_OK;
    }
    const cli_command_t *cmd = cli_find(argv[0], &ambiguous);
    if (cmd == NULL) {
        if (ambiguous) {
            io_printf("'%s' is ambiguous: ", argv[0]);
            list_prefix_matches(argv[0]);
        } else {
            (void)cli_error("unknown command '%s' (type 'help')", argv[0]);
        }
        return CLI_ERR;
    }
    int rc;
    if ((cmd->handler == NULL) && (cmd->subs != NULL)) {
        rc = cli_dispatch_subcmd(cmd->subs, argc, argv, cmd->name);
        if (rc == (CLI_ERR_USAGE + 100)) {
            rc = CLI_ERR;
        }
    } else if (cmd->handler != NULL) {
        rc = cmd->handler(argc, argv);
    } else {
        rc = CLI_ERR;
    }
    if (rc == CLI_ERR_USAGE) {
        (void)cli_usage(cmd->name, cmd->usage);
        if (cmd->subs != NULL) {
            print_subcmds(cmd->subs, cmd->name);
        }
    }
    return rc;
}

static int exec_one(char *cmdline)
{
    char    *argv[CLI_ARGV_MAX];
    char    *line = str_trim(cmdline);
    uint32_t argc = str_tokenize(line, argv, CLI_ARGV_MAX);
    return exec_argv((int)argc, argv);
}

int cli_execute(const char *line)
{
    char buf[CLI_LINE_MAX];
    int  rc       = CLI_OK;
    bool in_quote = false;
    char *start   = buf;

    (void)str_copy(buf, line, sizeof(buf));
    /* split on ';' outside double quotes */
    for (char *p = buf; ; p++) {
        if (*p == '"') {
            in_quote = !in_quote;
        }
        if (((*p == ';') && !in_quote) || (*p == '\0')) {
            const bool last = (*p == '\0');
            *p = '\0';
            rc = exec_one(start);
            if (last) {
                break;
            }
            start = p + 1;
        }
    }
    return rc;
}

bool cli_abort_requested(void)
{
    const int c = io_getc();
    return (c == KEY_CTRL_C) || (c == 'q') || (c == KEY_ESC) || (c == IO_CLOSED);
}

/* ------------------------------------------------------------------ */
/* History                                                             */
/* ------------------------------------------------------------------ */

static const char *hist_get(uint32_t age)       /* 0 = most recent */
{
    if (age >= s_cli.hist_count) {
        return NULL;
    }
    const uint32_t idx = (s_cli.hist_next + CLI_HISTORY_DEPTH - 1U - age) % CLI_HISTORY_DEPTH;
    return s_cli.history[idx];
}

static void hist_add(const char *line)
{
    const char *last = hist_get(0U);
    if ((line[0] == '\0') || ((last != NULL) && str_eq(last, line))) {
        return;                     /* skip empty lines and duplicates */
    }
    (void)str_copy(s_cli.history[s_cli.hist_next], line, CLI_LINE_MAX);
    s_cli.hist_next = (s_cli.hist_next + 1U) % CLI_HISTORY_DEPTH;
    if (s_cli.hist_count < CLI_HISTORY_DEPTH) {
        s_cli.hist_count++;
    }
}

static void hist_recall(int32_t direction)
{
    int32_t next = s_cli.hist_browse + direction;
    if (next < -1) {
        next = -1;
    }
    if (next >= (int32_t)s_cli.hist_count) {
        return;
    }
    s_cli.hist_browse = next;
    if (next < 0) {
        s_cli.line[0] = '\0';
    } else {
        (void)str_copy(s_cli.line, hist_get((uint32_t)next), CLI_LINE_MAX);
    }
    s_cli.len    = (uint32_t)str_len(s_cli.line);
    s_cli.cursor = s_cli.len;
    redraw_line();
}

/* ------------------------------------------------------------------ */
/* Line editing                                                        */
/* ------------------------------------------------------------------ */

static void insert_char(char c)
{
    if (s_cli.len >= (CLI_LINE_MAX - 1U)) {
        io_putc('\a');              /* bell: line full */
        return;
    }
    for (uint32_t i = s_cli.len; i > s_cli.cursor; i--) {
        s_cli.line[i] = s_cli.line[i - 1U];
    }
    s_cli.line[s_cli.cursor] = c;
    s_cli.len++;
    s_cli.cursor++;
    s_cli.line[s_cli.len] = '\0';
    if (s_cli.echo) {
        /* print from the new char to the end, then come back */
        io_write(&s_cli.line[s_cli.cursor - 1U], s_cli.len - s_cli.cursor + 1U);
        cursor_left(s_cli.len - s_cli.cursor);
    }
}

static void delete_at(uint32_t pos)
{
    if (pos >= s_cli.len) {
        return;
    }
    for (uint32_t i = pos; i < s_cli.len; i++) {
        s_cli.line[i] = s_cli.line[i + 1U];
    }
    s_cli.len--;
}

static void backspace(void)
{
    if (s_cli.cursor == 0U) {
        return;
    }
    s_cli.cursor--;
    delete_at(s_cli.cursor);
    if (s_cli.echo) {
        io_putc('\b');
        io_write(&s_cli.line[s_cli.cursor], s_cli.len - s_cli.cursor);
        io_putc(' ');
        cursor_left(s_cli.len - s_cli.cursor + 1U);
    }
}

static void delete_forward(void)
{
    if (s_cli.cursor < s_cli.len) {
        delete_at(s_cli.cursor);
        if (s_cli.echo) {
            io_write(&s_cli.line[s_cli.cursor], s_cli.len - s_cli.cursor);
            io_putc(' ');
            cursor_left(s_cli.len - s_cli.cursor + 1U);
        }
    }
}

static void kill_to_start(void)
{
    const uint32_t n = s_cli.cursor;
    for (uint32_t i = 0U; i <= (s_cli.len - n); i++) {
        s_cli.line[i] = s_cli.line[i + n];
    }
    s_cli.len   -= n;
    s_cli.cursor = 0U;
    redraw_line();
}

static void kill_to_end(void)
{
    s_cli.len = s_cli.cursor;
    s_cli.line[s_cli.len] = '\0';
    io_puts("\x1b[K");
}

static void delete_word(void)
{
    while ((s_cli.cursor > 0U) && (s_cli.line[s_cli.cursor - 1U] == ' ')) {
        backspace();
    }
    while ((s_cli.cursor > 0U) && (s_cli.line[s_cli.cursor - 1U] != ' ')) {
        backspace();
    }
}

/* ------------------------------------------------------------------ */
/* TAB completion                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *word;     /* what is typed so far */
    size_t      len;
    const char *first;    /* first match          */
    size_t      common;   /* common prefix length of all matches */
    uint32_t    count;
} completion_t;

static void completion_add(completion_t *c, const char *candidate)
{
    if (str_ncmp(candidate, c->word, c->len) != 0) {
        return;
    }
    if (c->count == 0U) {
        c->first  = candidate;
        c->common = str_len(candidate);
    } else {
        size_t k = 0U;
        while ((k < c->common) && (candidate[k] == c->first[k])) {
            k++;
        }
        c->common = k;
    }
    c->count++;
}

static void complete(void)
{
    char          *argv[3];
    char           tmp[CLI_LINE_MAX];
    completion_t   c      = { "", 0U, NULL, 0U, 0U };
    const bool     at_end = (s_cli.cursor == s_cli.len);
    const bool     space  = (s_cli.len > 0U) && (s_cli.line[s_cli.len - 1U] == ' ');

    if (!at_end) {
        return;
    }
    (void)str_copy(tmp, s_cli.line, sizeof(tmp));
    const uint32_t argc = str_tokenize(tmp, argv, 3U);
    const cli_subcmd_t *subs = NULL;

    if ((argc == 0U) || ((argc == 1U) && !space)) {
        /* completing a command name */
        c.word = (argc == 0U) ? "" : argv[0];
        c.len  = str_len(c.word);
        for (uint32_t g = 0U; g < s_cli.ngroups; g++) {
            for (uint32_t i = 0U; i < s_cli.groups[g]->count; i++) {
                completion_add(&c, s_cli.groups[g]->commands[i].name);
            }
        }
    } else if (((argc == 1U) && space) || ((argc == 2U) && !space)) {
        bool ambiguous;
        const cli_command_t *cmd = cli_find(argv[0], &ambiguous);
        if ((cmd == NULL) || (cmd->subs == NULL)) {
            return;
        }
        subs   = cmd->subs;
        c.word = (argc == 2U) ? argv[1] : "";
        c.len  = str_len(c.word);
        for (const cli_subcmd_t *s = subs; s->name != NULL; s++) {
            completion_add(&c, s->name);
        }
    } else {
        return;
    }

    if (c.count == 0U) {
        io_putc('\a');
        return;
    }
    /* extend with the common part (and a space if unique) */
    for (size_t k = c.len; k < c.common; k++) {
        insert_char(c.first[k]);
    }
    if (c.count == 1U) {
        insert_char(' ');
        return;
    }
    if (c.common == c.len) {
        /* nothing more to add: show the candidates */
        io_puts(IO_NEWLINE);
        if (subs == NULL) {
            for (uint32_t g = 0U; g < s_cli.ngroups; g++) {
                for (uint32_t i = 0U; i < s_cli.groups[g]->count; i++) {
                    const char *n = s_cli.groups[g]->commands[i].name;
                    if (str_ncmp(n, c.word, c.len) == 0) {
                        io_printf("%s  ", n);
                    }
                }
            }
        } else {
            for (const cli_subcmd_t *s = subs; s->name != NULL; s++) {
                if (str_ncmp(s->name, c.word, c.len) == 0) {
                    io_printf("%s  ", s->name);
                }
            }
        }
        io_puts(IO_NEWLINE);
        redraw_line();
    }
}

/* ------------------------------------------------------------------ */
/* Input state machine                                                 */
/* ------------------------------------------------------------------ */

static void enter(void)
{
    char line[CLI_LINE_MAX];

    io_puts(IO_NEWLINE);
    s_cli.line[s_cli.len] = '\0';
    (void)str_copy(line, s_cli.line, sizeof(line));
    s_cli.len         = 0U;
    s_cli.cursor      = 0U;
    s_cli.line[0]     = '\0';
    s_cli.hist_browse = -1;

    char *trimmed = str_trim(line);
    if (str_eq(trimmed, "!!")) {
        const char *last = hist_get(0U);
        if (last == NULL) {
            (void)cli_error("no history");
            cli_print_prompt();
            return;
        }
        (void)str_copy(line, last, sizeof(line));
        trimmed = line;
        io_printf("%s" IO_NEWLINE, trimmed);
    }
    hist_add(trimmed);
    (void)cli_execute(trimmed);
    cli_print_prompt();
}

static void handle_csi_final(char c)
{
    switch (c) {
    case 'A': hist_recall(+1); break;                         /* up    */
    case 'B': hist_recall(-1); break;                         /* down  */
    case 'C':                                                 /* right */
        if (s_cli.cursor < s_cli.len) {
            s_cli.cursor++;
            cursor_right(1U);
        }
        break;
    case 'D':                                                 /* left  */
        if (s_cli.cursor > 0U) {
            s_cli.cursor--;
            cursor_left(1U);
        }
        break;
    case 'H':                                                 /* home  */
        cursor_left(s_cli.cursor);
        s_cli.cursor = 0U;
        break;
    case 'F':                                                 /* end   */
        cursor_right(s_cli.len - s_cli.cursor);
        s_cli.cursor = s_cli.len;
        break;
    case '~':
        if (s_cli.esc_param == 3U) {                          /* Delete */
            delete_forward();
        } else if ((s_cli.esc_param == 1U) || (s_cli.esc_param == 7U)) {
            handle_csi_final('H');
        } else if ((s_cli.esc_param == 4U) || (s_cli.esc_param == 8U)) {
            handle_csi_final('F');
        } else {
            /* other keys (PgUp...) ignored */
        }
        break;
    default:
        break;
    }
}

void cli_input_char(char c)
{
    /* escape sequences (arrow keys etc.) */
    if (s_cli.esc == ESC_START) {
        s_cli.esc       = ((c == '[') || (c == 'O')) ? ESC_CSI : ESC_NONE;
        s_cli.esc_param = 0U;
        return;
    }
    if (s_cli.esc == ESC_CSI) {
        if (chr_is_digit(c)) {
            s_cli.esc_param = (s_cli.esc_param * 10U) + (uint32_t)(c - '0');
            return;
        }
        s_cli.esc = ESC_NONE;
        handle_csi_final(c);
        return;
    }

    const char prev = s_cli.last_char;
    s_cli.last_char = c;

    switch (c) {
    case KEY_CR:
        enter();
        break;
    case KEY_LF:
        if (prev != KEY_CR) {        /* CRLF counts once, lone LF = Enter */
            enter();
        }
        break;
    case KEY_ESC:
        s_cli.esc = ESC_START;
        break;
    case KEY_BACKSPACE:
    case KEY_DEL:
        backspace();
        break;
    case KEY_TAB:
        complete();
        break;
    case KEY_CTRL_A:
        handle_csi_final('H');
        break;
    case KEY_CTRL_E:
        handle_csi_final('F');
        break;
    case KEY_CTRL_U:
        kill_to_start();
        break;
    case KEY_CTRL_K:
        kill_to_end();
        break;
    case KEY_CTRL_W:
        delete_word();
        break;
    case KEY_CTRL_P:
        hist_recall(+1);
        break;
    case KEY_CTRL_N:
        hist_recall(-1);
        break;
    case KEY_CTRL_D:
        delete_forward();
        break;
    case KEY_CTRL_L:
        io_ansi_clear_screen();
        redraw_line();
        break;
    case KEY_CTRL_C:
        io_puts("^C" IO_NEWLINE);
        s_cli.len    = 0U;
        s_cli.cursor = 0U;
        s_cli.line[0] = '\0';
        cli_print_prompt();
        break;
    default:
        if (chr_is_print(c)) {
            insert_char(c);
        }
        break;
    }
}

void cli_poll(void)
{
    int c;
    /* bounded so a flood of input cannot starve the main loop */
    for (uint32_t n = 0U; n < 32U; n++) {
        c = io_getc();
        if (c < 0) {
            break;            /* IO_EOF or IO_CLOSED */
        }
        cli_input_char((char)c);
    }
}

/* ------------------------------------------------------------------ */
/* Built-in commands                                                   */
/* ------------------------------------------------------------------ */

static void help_command(const cli_command_t *c)
{
    char left[CLI_LINE_MAX];
    (void)fmt_snprintf(left, sizeof(left), "%s %s", c->name, (c->usage != NULL) ? c->usage : "");
    print_help_line(left, c->help);
}

static int cmd_help(int argc, char *argv[])
{
    if (argc >= 2) {
        bool ambiguous;
        const cli_command_t *c = cli_find(argv[1], &ambiguous);
        if (c == NULL) {
            return cli_error("no command '%s'", argv[1]);
        }
        help_command(c);
        if (c->subs != NULL) {
            print_subcmds(c->subs, c->name);
        }
        return CLI_OK;
    }
    for (uint32_t g = 0U; g < s_cli.ngroups; g++) {
        const cli_group_t *grp = s_cli.groups[g];
        io_ansi_color(IO_COLOR_CYAN);
        io_printf("%s" IO_NEWLINE, grp->name);
        io_ansi_color(IO_COLOR_RESET);
        for (uint32_t i = 0U; i < grp->count; i++) {
            help_command(&grp->commands[i]);
        }
    }
    io_println("keys: TAB complete, up/down history, Ctrl-A/E/U/K/W/L, '!!' last, ';' chains");
    return CLI_OK;
}

static int cmd_history(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    for (uint32_t age = s_cli.hist_count; age > 0U; age--) {
        io_printf("%3lu  %s" IO_NEWLINE, (unsigned long)(s_cli.hist_count - age + 1U), hist_get(age - 1U));
    }
    return CLI_OK;
}

static int cmd_repeat(int argc, char *argv[])
{
    uint32_t n;
    if (argc < 3) {
        return CLI_ERR_USAGE;
    }
    if (!cli_arg_u32(argv[1], &n)) {
        return CLI_ERR;
    }
    for (uint32_t i = 0U; i < n; i++) {
        if (exec_argv(argc - 2, &argv[2]) != CLI_OK) {
            return CLI_ERR;
        }
        if (cli_abort_requested()) {
            io_println("aborted");
            break;
        }
    }
    return CLI_OK;
}

static int cmd_watch(int argc, char *argv[])
{
    uint32_t period;
    if (argc < 3) {
        return CLI_ERR_USAGE;
    }
    if (!cli_arg_u32_range(argv[1], 10U, 3600000U, &period)) {
        return CLI_ERR;
    }
    io_println("(press q or Ctrl-C to stop)");
    for (;;) {
        const uint32_t start = hal_time_ms();
        (void)exec_argv(argc - 2, &argv[2]);
        while ((hal_time_ms() - start) < period) {
            if (cli_abort_requested()) {
                return CLI_OK;
            }
        }
    }
}

static int cmd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    io_ansi_clear_screen();
    return CLI_OK;
}

static const cli_command_t builtin_cmds[] = {
    { "help",    cmd_help,    "[cmd]",          "list commands, or details of one", NULL },
    { "history", cmd_history, "",               "show the command history",          NULL },
    { "repeat",  cmd_repeat,  "<n> <cmd...>",   "run a command n times",             NULL },
    { "watch",   cmd_watch,   "<ms> <cmd...>",  "run a command periodically (q stops)", NULL },
    { "clear",   cmd_clear,   "",               "clear the screen (also Ctrl-L)",    NULL },
};

static const cli_group_t builtin_group = {
    "shell", builtin_cmds, sizeof(builtin_cmds) / sizeof(builtin_cmds[0])
};

const cli_group_t *cli_builtin_group(void)
{
    return &builtin_group;
}

/* ------------------------------------------------------------------ */
/* Setup                                                               */
/* ------------------------------------------------------------------ */

void cli_init(const char *prompt)
{
    s_cli.prompt      = (prompt != NULL) ? prompt : "> ";
    s_cli.len         = 0U;
    s_cli.cursor      = 0U;
    s_cli.esc         = ESC_NONE;
    s_cli.echo        = true;
    s_cli.hist_browse = -1;
    s_cli.ngroups     = 0U;
    (void)cli_register(&builtin_group);
}

bool cli_register(const cli_group_t *group)
{
    if ((group == NULL) || (s_cli.ngroups >= CLI_GROUPS_MAX)) {
        return false;
    }
    s_cli.groups[s_cli.ngroups++] = group;
    return true;
}

void cli_set_echo(bool enable)
{
    s_cli.echo = enable;
}

void cli_set_prompt(const char *prompt)
{
    s_cli.prompt = prompt;
}

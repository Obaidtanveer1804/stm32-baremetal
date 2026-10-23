/**
 * @file    cli.h
 * @brief   Interactive shell for the serial console, Linux-terminal
 *          style, sized for an 8 KiB RAM MCU.
 *
 * Features
 *  - line editing: left/right, Home/End, Backspace/Delete, Ctrl-A/E/U/K/W/L
 *  - history with up/down arrows, "!!" repeats the last command
 *  - TAB completion of commands and of sub-commands
 *  - unique-prefix abbreviations ("gp set PC8" runs "gpio set PC8")
 *  - several commands per line separated by ';'
 *  - quoted arguments ("echo "two words"")
 *  - built-ins: help, history, repeat N <cmd>, watch <ms> <cmd>
 *  - Ctrl-C (or 'q') aborts long running commands (cli_abort_requested)
 *
 * Non blocking: call cli_poll() from the main loop (or a scheduler
 * task); it consumes the characters available and returns at once.
 *
 * Commands are registered by groups, so each application links only
 * the groups it needs:
 *
 *   cli_init("stm32> ");
 *   cli_register(cmd_gpio_group());
 *   cli_register(cmd_sys_group());
 *   for (;;) { cli_poll(); other_work(); }
 */
#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stdbool.h>

#ifndef CLI_LINE_MAX
#define CLI_LINE_MAX      (96U)    /* longest input line            */
#endif
#ifndef CLI_HISTORY_DEPTH
#define CLI_HISTORY_DEPTH (8U)     /* lines kept for the up arrow   */
#endif
#ifndef CLI_ARGV_MAX
#define CLI_ARGV_MAX      (16U)    /* words per command             */
#endif
#ifndef CLI_GROUPS_MAX
#define CLI_GROUPS_MAX    (24U)
#endif

/** Return codes of command handlers. */
#define CLI_OK            (0)
#define CLI_ERR           (1)      /**< failed, message already printed */
#define CLI_ERR_USAGE     (2)      /**< bad arguments: prints the usage */

typedef int (*cli_handler_t)(int argc, char *argv[]);

/** Sub-command, for "gpio set PC8"-like commands (used for TAB too). */
typedef struct {
    const char   *name;
    cli_handler_t handler;    /**< receives argv starting at the sub-command */
    const char   *usage;
    const char   *help;
} cli_subcmd_t;

typedef struct {
    const char         *name;
    cli_handler_t       handler;
    const char         *usage;    /**< e.g. "<pin> [0|1]"              */
    const char         *help;     /**< one line description            */
    const cli_subcmd_t *subs;     /**< NULL or table ended by {NULL}   */
} cli_command_t;

typedef struct {
    const char          *name;    /**< group title shown by "help"     */
    const cli_command_t *commands;
    uint32_t             count;
} cli_group_t;

void cli_init(const char *prompt);
/** Add a group of commands. Returns false if the table is full. */
bool cli_register(const cli_group_t *group);
/** Process pending input characters. Never blocks. */
void cli_poll(void);
/** Feed one character (for other input sources, or tests). */
void cli_input_char(char c);
/** Execute a full command line as if typed (';' separated). */
int  cli_execute(const char *line);
void cli_print_prompt(void);
void cli_set_echo(bool enable);
void cli_set_prompt(const char *prompt);

/** Dispatch argv[1] to a sub-command table (prints errors itself). */
int  cli_dispatch_subcmd(const cli_subcmd_t *subs, int argc, char *argv[], const char *parent);

/** True when the user pressed Ctrl-C or 'q' (non-blocking, consumes input). */
bool cli_abort_requested(void);

/** Usage error helper: prints "usage: <cmd> <usage>" and returns CLI_ERR_USAGE. */
int  cli_usage(const char *name, const char *usage);
int  cli_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/** Find a command by exact name or unique prefix. */
const cli_command_t *cli_find(const char *name, bool *ambiguous);

/* --------------- argument parsing helpers (print on error) ------- */
bool cli_arg_u32(const char *s, uint32_t *v);
bool cli_arg_i32(const char *s, int32_t *v);
bool cli_arg_u32_range(const char *s, uint32_t lo, uint32_t hi, uint32_t *v);
bool cli_arg_bool(const char *s, bool *v);

/**
 * Shared work memory for commands. Only one command runs at a time, so
 * big temporary buffers (FFT, benchmarks, dumps) share one static block
 * instead of each keeping its own: RAM is the scarce resource on an
 * 8 KiB part. Returns NULL if @p size is larger than CLI_SCRATCH_SIZE.
 * The content is undefined at each command start.
 */
#ifndef CLI_SCRATCH_SIZE
#define CLI_SCRATCH_SIZE  (2048U)
#endif
void *cli_scratch(uint32_t size);

/** The built-in commands of the shell itself. */
const cli_group_t *cli_builtin_group(void);

#endif /* CLI_H */

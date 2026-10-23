/**
 * @file    sim_core_pc.c
 * @brief   PC port engine: 1 ms "SysTick" with SIGALRM, interrupt
 *          masking with sigprocmask, raw terminal, logging, and the
 *          hal_sys / hal_irq / hal_time implementations.
 *
 * Only async-signal-safe calls are used on the interrupt path
 * (write, read, clock_gettime): no printf/malloc in the handler.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "sim_internal.h"
#include "fmt.h"
#include "strutil.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define SIM_TICK_US           (1000L)
#define SIM_MAX_CATCHUP_MS    (100U)
#define SIM_CLOCK_HZ          (24000000UL)   /* pretend to be the F100 */
#define SIM_RAM_SIZE          (8U * 1024U)
#define SIM_RAM_BASE          (0x20000000UL)
#define SIM_FLASH_KB          (128U)
#define SIM_LOG_BUF_SIZE      (256U)

static volatile sig_atomic_t s_started;
static volatile sig_atomic_t s_in_isr;
static volatile uint32_t     s_ms;
static struct timespec       s_t0;
static struct termios        s_saved_termios;
static bool                  s_termios_saved;
static int                   s_saved_fl = -1;
static bool                  s_stdin_tty;
static bool                  s_stdout_tty;
static uint32_t              s_log_mask;
static hal_tick_hook_t       s_tick_hook;
static uint32_t              s_reset_cause = HAL_RESET_POWER_ON;
static uint8_t               s_ram[SIM_RAM_SIZE];

/* ------------------------------------------------------------------ */
/* Low level helpers                                                   */
/* ------------------------------------------------------------------ */

void sim_raw_write(const char *data, size_t len)
{
    while (len > 0U) {
        const ssize_t n = write(STDOUT_FILENO, data, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN) {
                continue;           /* terminal busy: retry */
            }
            return;
        }
        data += n;
        len  -= (size_t)n;
    }
}

uint64_t sim_now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    const int64_t sec  = (int64_t)(t.tv_sec - s_t0.tv_sec);
    const int64_t nsec = (int64_t)(t.tv_nsec - s_t0.tv_nsec);
    return (uint64_t)((sec * 1000000LL) + (nsec / 1000LL));
}

bool sim_in_isr(void)
{
    return s_in_isr != 0;
}

bool sim_stdin_is_tty(void)
{
    return s_stdin_tty;
}

/* ------------------------------------------------------------------ */
/* Terminal                                                            */
/* ------------------------------------------------------------------ */

static void terminal_restore(void)
{
    if (s_termios_saved) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &s_saved_termios);
    }
    if (s_saved_fl >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, s_saved_fl);
    }
}

static void terminal_setup(void)
{
    s_stdin_tty  = isatty(STDIN_FILENO) != 0;
    s_stdout_tty = isatty(STDOUT_FILENO) != 0;

    if (s_stdin_tty && (tcgetattr(STDIN_FILENO, &s_saved_termios) == 0)) {
        struct termios raw = s_saved_termios;
        s_termios_saved = true;
        /* like a serial terminal: byte at a time, no local echo,
         * Enter sends '\r'. ISIG kept so Ctrl-C still quits. */
        raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
        raw.c_iflag &= (tcflag_t)~(ICRNL | IXON);
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 0;
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    s_saved_fl = fcntl(STDIN_FILENO, F_GETFL);
    if (s_saved_fl >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, s_saved_fl | O_NONBLOCK);
    }
}

/* ------------------------------------------------------------------ */
/* Tick (simulated SysTick interrupt)                                  */
/* ------------------------------------------------------------------ */

static void tick_handler(int sig)
{
    const int saved_errno = errno;   /* handlers must not clobber errno */
    (void)sig;

    s_in_isr = 1;
    const uint64_t now_us    = sim_now_us();
    const uint32_t target_ms = (uint32_t)(now_us / 1000U);
    uint32_t       steps     = 0U;

    /* one hook call per elapsed millisecond, like a real 1 kHz tick */
    while ((int32_t)(target_ms - s_ms) > 0) {
        s_ms++;
        sim_time_tick(s_ms);
        sim_wdg_tick(s_ms);
        if (++steps >= SIM_MAX_CATCHUP_MS) {
            s_ms = target_ms;           /* host was suspended: jump */
            break;
        }
    }
    sim_uart_tick();
    sim_timer_tick(now_us);
    sim_rtc_tick();
    s_in_isr = 0;
    errno = saved_errno;
}

static void quit_handler(int sig)
{
    terminal_restore();
    sim_raw_write("\r\n[sim] interrupted\r\n", 21U);
    _exit(128 + sig);
}

static void read_reset_cause_env(void)
{
    const char *cause = getenv("SIM_RESET_CAUSE");
    if (cause == NULL) {
        s_reset_cause = HAL_RESET_POWER_ON;
    } else if (str_eq(cause, "software")) {
        s_reset_cause = HAL_RESET_SOFTWARE;
    } else if (str_eq(cause, "iwdg")) {
        s_reset_cause = HAL_RESET_IWDG;
    } else {
        s_reset_cause = HAL_RESET_PIN;
    }
    (void)unsetenv("SIM_RESET_CAUSE");
}

void sim_core_ensure_started(void)
{
    struct sigaction sa;
    struct itimerval it;

    if (s_started != 0) {
        return;
    }
    s_started = 1;
    clock_gettime(CLOCK_MONOTONIC, &s_t0);
    read_reset_cause_env();
    terminal_setup();
    (void)atexit(terminal_restore);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = quit_handler;
    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = tick_handler;
    (void)sigaction(SIGALRM, &sa, NULL);

    it.it_interval.tv_sec  = 0;
    it.it_interval.tv_usec = SIM_TICK_US;
    it.it_value            = it.it_interval;
    (void)setitimer(ITIMER_REAL, &it, NULL);
}

void sim_shutdown(int code)
{
    struct itimerval off = { { 0, 0 }, { 0, 0 } };
    (void)setitimer(ITIMER_REAL, &off, NULL);
    sim_flash_save();
    terminal_restore();
    exit(code);
}

/* ------------------------------------------------------------------ */
/* Logging                                                             */
/* ------------------------------------------------------------------ */

static const char *const log_names[] = {
    "gpio", "pwm", "adc", "spi", "i2c", "uart", "timer", "flash", "exti", "sys"
};

void sim_set_log(uint32_t mask)
{
    s_log_mask = mask;
}

uint32_t sim_get_log(void)
{
    return s_log_mask;
}

const char *sim_log_name(uint32_t bit)
{
    for (uint32_t i = 0U; i < (sizeof(log_names) / sizeof(log_names[0])); i++) {
        if (bit == (1U << i)) {
            return log_names[i];
        }
    }
    return "?";
}

bool sim_parse_log(const char *name, uint32_t *mask)
{
    if (str_ieq(name, "all"))  { *mask = SIM_LOG_ALL; return true; }
    if (str_ieq(name, "none")) { *mask = 0U;          return true; }
    for (uint32_t i = 0U; i < (sizeof(log_names) / sizeof(log_names[0])); i++) {
        if (str_ieq(name, log_names[i])) {
            *mask = 1U << i;
            return true;
        }
    }
    return false;
}

void sim_printf(const char *fmt, ...)
{
    char    buf[SIM_LOG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    (void)fmt_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sim_raw_write(buf, str_len(buf));
}

void sim_log(uint32_t category, const char *fmt, ...)
{
    char    buf[SIM_LOG_BUF_SIZE];
    size_t  n = 0U;
    va_list args;

    if ((s_log_mask & category) == 0U) {
        return;
    }
    n += str_copy(&buf[n], s_stdout_tty ? "\x1b[2m[sim] " : "[sim] ", sizeof(buf) - n);
    va_start(args, fmt);
    (void)fmt_vsnprintf(&buf[n], sizeof(buf) - n, fmt, args);
    va_end(args);
    (void)str_cat(buf, s_stdout_tty ? "\x1b[0m\r\n" : "\r\n", sizeof(buf));
    n = str_len(buf);
    sim_raw_write(buf, n);
}

/* ------------------------------------------------------------------ */
/* hal_time                                                            */
/* ------------------------------------------------------------------ */

void hal_time_init(void)
{
    sim_core_ensure_started();
}

uint32_t hal_time_ms(void)
{
    sim_core_ensure_started();
    return s_ms;
}

uint32_t hal_time_us(void)
{
    sim_core_ensure_started();
    return (uint32_t)sim_now_us();
}

static void sleep_us(long us)
{
    struct timespec ts = { 0, us * 1000L };
    while ((nanosleep(&ts, &ts) != 0) && (errno == EINTR)) {
        /* the tick signal interrupted us: sleep the remaining time */
    }
}

void hal_delay_ms(uint32_t ms)
{
    const uint32_t start = hal_time_ms();
    while ((hal_time_ms() - start) < ms) {
        sleep_us(200L);
    }
}

void hal_delay_us(uint32_t us)
{
    const uint32_t start = hal_time_us();
    while ((hal_time_us() - start) < us) {
        /* busy wait like the MCU (short delays only) */
    }
}

void hal_time_set_tick_hook(hal_tick_hook_t hook)
{
    s_tick_hook = hook;
}

void sim_time_tick(uint32_t now_ms)
{
    if (s_tick_hook != NULL) {
        s_tick_hook(now_ms);
    }
}

/* ------------------------------------------------------------------ */
/* hal_irq                                                             */
/* ------------------------------------------------------------------ */

uint32_t hal_irq_save(void)
{
    sigset_t block;
    sigset_t old;
    sigemptyset(&block);
    sigaddset(&block, SIGALRM);
    (void)sigprocmask(SIG_BLOCK, &block, &old);
    /* 1 = interrupts were already masked */
    return (sigismember(&old, SIGALRM) == 1) ? 1U : 0U;
}

void hal_irq_restore(uint32_t state)
{
    if (state == 0U) {
        hal_irq_enable_all();
    }
}

void hal_irq_enable_all(void)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    (void)sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void hal_irq_disable_all(void)
{
    (void)hal_irq_save();
}

/* ------------------------------------------------------------------ */
/* hal_sys                                                             */
/* ------------------------------------------------------------------ */

void hal_sys_init(void)
{
    sim_core_ensure_started();
    sim_log(SIM_LOG_SYS, "clock %lu Hz (simulated STM32F100)", (unsigned long)SIM_CLOCK_HZ);
}

uint32_t hal_sys_clock_hz(void)
{
    return SIM_CLOCK_HZ;
}

const char *hal_sys_name(void)
{
    return "PC simulation of STM32F100";
}

/* Simulated reset: re-execute the program with the same arguments. */
static void reexec(const char *cause)
{
    static char  cmdline[4096];
    static char *argv[64];
    int          argc = 0;
    ssize_t      len  = 0;

    const int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd >= 0) {
        len = read(fd, cmdline, sizeof(cmdline) - 1U);
        (void)close(fd);
    }
    if (len <= 0) {
        return;
    }
    cmdline[len] = '\0';
    for (ssize_t i = 0; (i < len) && (argc < 63); i += (ssize_t)str_len(&cmdline[i]) + 1) {
        argv[argc++] = &cmdline[i];
    }
    argv[argc] = NULL;
    (void)setenv("SIM_RESET_CAUSE", cause, 1);
    (void)execv("/proc/self/exe", argv);
}

void sim_reset_with_cause(const char *cause)
{
    struct itimerval off = { { 0, 0 }, { 0, 0 } };
    (void)setitimer(ITIMER_REAL, &off, NULL);
    sim_flash_save();
    terminal_restore();
    sim_raw_write("\r\n[sim] *** RESET ***\r\n\r\n", 25U);
    if (s_stdin_tty) {
        reexec(cause);          /* returns only on failure */
    }
    _exit(0);
}

void hal_sys_reset(void)
{
    sim_reset_with_cause("software");
}

uint32_t hal_sys_reset_cause(void)
{
    sim_core_ensure_started();
    return s_reset_cause;
}

void hal_sys_unique_id(uint32_t id[3])
{
    id[0] = 0x0047002EUL;
    id[1] = 0x31365111UL;
    id[2] = 0x37383735UL;
}

uint32_t hal_sys_flash_size_kb(void)
{
    return SIM_FLASH_KB;
}

uint32_t hal_sys_ram_size_kb(void)
{
    return SIM_RAM_SIZE / 1024U;
}

void hal_sys_cycles_init(void)
{
    sim_core_ensure_started();
}

uint32_t hal_sys_cycles(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    /* nanoseconds * 24 MHz / 1e9, the clock of the simulated F100 */
    const uint64_t ns = ((uint64_t)t.tv_sec * 1000000000ULL) + (uint64_t)t.tv_nsec;
    return (uint32_t)((ns * (SIM_CLOCK_HZ / 1000000U)) / 1000U);
}

void hal_sys_idle(void)
{
    sim_core_ensure_started();
    /* piped script consumed and nothing left to process: power off */
    if (sim_stdin_eof() && (hal_uart_rx_available(BOARD_CONSOLE_UART) == 0U)) {
        sim_shutdown(0);
    }
    sleep_us(SIM_TICK_US);     /* "wait for interrupt" */
}

uint32_t hal_sys_stack_free(void)
{
    return 0U;                 /* not meaningful on the PC */
}

/* Fake address space: 8 KiB SRAM, GPIO registers, user flash. */
bool hal_sys_mem_read32(uint32_t addr, uint32_t *value)
{
    if (((addr & 3U) == 0U) && (addr >= SIM_RAM_BASE) && ((addr + 4U) <= (SIM_RAM_BASE + SIM_RAM_SIZE))) {
        const uint8_t *p = &s_ram[addr - SIM_RAM_BASE];
        *value = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        return true;
    }
    if (sim_gpio_reg_read(addr, value) || sim_timer_reg_read(addr, value)) {
        return true;
    }
    uint8_t b[4];
    if (((addr & 3U) == 0U) && sim_flash_mem_read8(addr, &b[0]) && sim_flash_mem_read8(addr + 1U, &b[1]) &&
        sim_flash_mem_read8(addr + 2U, &b[2]) && sim_flash_mem_read8(addr + 3U, &b[3])) {
        *value = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
        return true;
    }
    return false;
}

bool hal_sys_mem_write32(uint32_t addr, uint32_t value)
{
    if (((addr & 3U) == 0U) && (addr >= SIM_RAM_BASE) && ((addr + 4U) <= (SIM_RAM_BASE + SIM_RAM_SIZE))) {
        uint8_t *p = &s_ram[addr - SIM_RAM_BASE];
        p[0] = (uint8_t)value;
        p[1] = (uint8_t)(value >> 8);
        p[2] = (uint8_t)(value >> 16);
        p[3] = (uint8_t)(value >> 24);
        return true;
    }
    return sim_gpio_reg_write(addr, value) || sim_timer_reg_write(addr, value);
}

bool hal_sys_mem_read8(uint32_t addr, uint8_t *value)
{
    if ((addr >= SIM_RAM_BASE) && (addr < (SIM_RAM_BASE + SIM_RAM_SIZE))) {
        *value = s_ram[addr - SIM_RAM_BASE];
        return true;
    }
    uint32_t word;
    if (sim_gpio_reg_read(addr & ~3U, &word)) {
        *value = (uint8_t)(word >> ((addr & 3U) * 8U));
        return true;
    }
    return sim_flash_mem_read8(addr, value);
}

void sim_status(void)
{
    sim_gpio_status();
    sim_pwm_status();
    sim_timer_status();
    sim_adc_status();
}

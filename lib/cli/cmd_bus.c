/**
 * @file    cmd_bus.c
 * @brief   Bus commands, like i2c-tools / spidev on Linux:
 *
 *   i2c init 1 100000          i2c scan 1
 *   i2c read 1 0x48 2          i2c write 1 0x50 0x00 0xAB 0xCD
 *   i2c reg 1 0x68 0x00 7      i2c dump 1 0x50
 *   spi init 1 1000000 0       spi xfer 1 PA4 0x9F 0 0 0
 *   uart init 2 9600           uart send 2 "hello"     uart recv 2
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "hexdump.h"
#include "strutil.h"

#define BUS_BUF_MAX  (32U)

/* argv entries -> byte array; returns count or -1 */
static int32_t parse_bytes(int argc, char *argv[], uint8_t *out, uint32_t max)
{
    if ((uint32_t)argc > max) {
        (void)cli_error("at most %lu bytes", (unsigned long)max);
        return -1;
    }
    for (int i = 0; i < argc; i++) {
        uint32_t v;
        if (!cli_arg_u32_range(argv[i], 0U, 0xFFU, &v)) {
            return -1;
        }
        out[i] = (uint8_t)v;
    }
    return argc;
}

static void print_bytes(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++) {
        io_printf("%02X ", (unsigned)data[i]);
    }
    io_puts(IO_NEWLINE);
}

/* ------------------------------- I2C ------------------------------ */

static bool parse_i2c(const char *s, hal_i2c_t *bus)
{
    uint32_t n;
    if (!cli_arg_u32_range(s, 1U, HAL_I2C_COUNT, &n)) {
        return false;
    }
    *bus = (hal_i2c_t)(n - 1U);
    return true;
}

static bool parse_addr7(const char *s, uint8_t *addr)
{
    uint32_t a;
    if (!cli_arg_u32_range(s, 0x03U, 0x77U, &a)) {
        return false;
    }
    *addr = (uint8_t)a;
    return true;
}

static int report(hal_status_t st)
{
    return (st == HAL_OK) ? CLI_OK : cli_error("%s", hal_status_str(st));
}

static int sub_i2c_init(int argc, char *argv[])
{
    hal_i2c_t bus;
    uint32_t  hz = 100000U;
    if ((argc < 2) || !parse_i2c(argv[1], &bus)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 10000U, 400000U, &hz)) {
        return CLI_ERR;
    }
    return report(hal_i2c_init(bus, hz));
}

static int sub_i2c_scan(int argc, char *argv[])
{
    hal_i2c_t bus;
    uint32_t  found = 0U;
    if ((argc != 2) || !parse_i2c(argv[1], &bus)) {
        return CLI_ERR_USAGE;
    }
    /* same layout as Linux i2cdetect */
    io_println("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
    for (uint32_t row = 0U; row < 0x80U; row += 16U) {
        io_printf("%02lx: ", (unsigned long)row);
        for (uint32_t col = 0U; col < 16U; col++) {
            const uint32_t a = row + col;
            if ((a < 0x03U) || (a > 0x77U)) {
                io_puts("   ");
            } else if (hal_i2c_probe(bus, (uint8_t)a)) {
                io_printf("%02lx ", (unsigned long)a);
                found++;
            } else {
                io_puts("-- ");
            }
        }
        io_puts(IO_NEWLINE);
    }
    io_printf("%lu device(s)" IO_NEWLINE, (unsigned long)found);
    return CLI_OK;
}

static int sub_i2c_read(int argc, char *argv[])
{
    hal_i2c_t bus;
    uint8_t   addr;
    uint32_t  n;
    uint8_t   buf[BUS_BUF_MAX];
    if ((argc != 4) || !parse_i2c(argv[1], &bus) || !parse_addr7(argv[2], &addr) ||
        !cli_arg_u32_range(argv[3], 1U, BUS_BUF_MAX, &n)) {
        return CLI_ERR_USAGE;
    }
    const hal_status_t st = hal_i2c_read(bus, addr, buf, n);
    if (st == HAL_OK) {
        print_bytes(buf, n);
    }
    return report(st);
}

static int sub_i2c_write(int argc, char *argv[])
{
    hal_i2c_t bus;
    uint8_t   addr;
    uint8_t   buf[BUS_BUF_MAX];
    if ((argc < 4) || !parse_i2c(argv[1], &bus) || !parse_addr7(argv[2], &addr)) {
        return CLI_ERR_USAGE;
    }
    const int32_t n = parse_bytes(argc - 3, &argv[3], buf, BUS_BUF_MAX);
    if (n < 0) {
        return CLI_ERR;
    }
    return report(hal_i2c_write(bus, addr, buf, (size_t)n));
}

static int sub_i2c_reg(int argc, char *argv[])
{
    hal_i2c_t bus;
    uint8_t   addr;
    uint32_t  reg;
    uint32_t  n = 1U;
    uint8_t   buf[BUS_BUF_MAX];
    if ((argc < 4) || !parse_i2c(argv[1], &bus) || !parse_addr7(argv[2], &addr) ||
        !cli_arg_u32_range(argv[3], 0U, 0xFFU, &reg)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 5) && !cli_arg_u32_range(argv[4], 1U, BUS_BUF_MAX, &n)) {
        return CLI_ERR;
    }
    const hal_status_t st = hal_i2c_reg_read(bus, addr, (uint8_t)reg, buf, n);
    if (st == HAL_OK) {
        io_printf("[%02lX] ", (unsigned long)reg);
        print_bytes(buf, n);
    }
    return report(st);
}

static int sub_i2c_set(int argc, char *argv[])
{
    hal_i2c_t bus;
    uint8_t   addr;
    uint32_t  reg;
    uint32_t  val;
    if ((argc != 5) || !parse_i2c(argv[1], &bus) || !parse_addr7(argv[2], &addr) ||
        !cli_arg_u32_range(argv[3], 0U, 0xFFU, &reg) || !cli_arg_u32_range(argv[4], 0U, 0xFFU, &val)) {
        return CLI_ERR_USAGE;
    }
    return report(hal_i2c_reg_write8(bus, addr, (uint8_t)reg, (uint8_t)val));
}

static int sub_i2c_dump(int argc, char *argv[])
{
    hal_i2c_t bus;
    uint8_t   addr;
    uint8_t   page[16];
    if ((argc != 3) || !parse_i2c(argv[1], &bus) || !parse_addr7(argv[2], &addr)) {
        return CLI_ERR_USAGE;
    }
    for (uint32_t r = 0U; r < 256U; r += sizeof(page)) {
        const hal_status_t st = hal_i2c_reg_read(bus, addr, (uint8_t)r, page, sizeof(page));
        if (st != HAL_OK) {
            return report(st);
        }
        hexdump(page, sizeof(page), r);
    }
    return CLI_OK;
}

static const cli_subcmd_t i2c_subs[] = {
    { "init",  sub_i2c_init,  "<1|2> [hz]",             "configure the bus (100k/400k)" },
    { "scan",  sub_i2c_scan,  "<bus>",                  "find devices (like i2cdetect)" },
    { "read",  sub_i2c_read,  "<bus> <addr> <n>",       "read n bytes" },
    { "write", sub_i2c_write, "<bus> <addr> <bytes...>","write bytes" },
    { "reg",   sub_i2c_reg,   "<bus> <addr> <reg> [n]", "read register(s)" },
    { "set",   sub_i2c_set,   "<bus> <addr> <reg> <v>", "write one register" },
    { "dump",  sub_i2c_dump,  "<bus> <addr>",           "read all 256 registers" },
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------- SPI ------------------------------ */

static bool parse_spi(const char *s, hal_spi_t *bus)
{
    uint32_t n;
    if (!cli_arg_u32_range(s, 1U, HAL_SPI_COUNT, &n)) {
        return false;
    }
    *bus = (hal_spi_t)(n - 1U);
    return true;
}

static int sub_spi_init(int argc, char *argv[])
{
    hal_spi_t        bus;
    uint32_t         hz   = 1000000U;
    uint32_t         mode = 0U;
    hal_spi_config_t cfg;
    if ((argc < 2) || !parse_spi(argv[1], &bus)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 1000U, 36000000U, &hz)) {
        return CLI_ERR;
    }
    if ((argc >= 4) && !cli_arg_u32_range(argv[3], 0U, 3U, &mode)) {
        return CLI_ERR;
    }
    cfg.max_hz    = hz;
    cfg.mode      = (hal_spi_mode_t)mode;
    cfg.lsb_first = (argc >= 5) && str_eq(argv[4], "lsb");
    const hal_status_t st = hal_spi_init(bus, &cfg);
    if (st == HAL_OK) {
        io_printf("spi%lu: SCK %lu Hz, mode %lu" IO_NEWLINE, (unsigned long)bus + 1U,
                  (unsigned long)hal_spi_get_hz(bus), (unsigned long)mode);
    }
    return report(st);
}

static int sub_spi_xfer(int argc, char *argv[])
{
    hal_spi_t bus;
    hal_pin_t cs;
    uint8_t   tx[BUS_BUF_MAX];
    uint8_t   rx[BUS_BUF_MAX];
    if ((argc < 4) || !parse_spi(argv[1], &bus)) {
        return CLI_ERR_USAGE;
    }
    if (!hal_gpio_parse(argv[2], &cs)) {
        return cli_error("chip select '%s' is not a pin", argv[2]);
    }
    const int32_t n = parse_bytes(argc - 3, &argv[3], tx, BUS_BUF_MAX);
    if (n < 0) {
        return CLI_ERR;
    }
    hal_gpio_mode(cs, HAL_GPIO_OUTPUT);
    hal_gpio_clear(cs);                       /* CS active low */
    hal_spi_transfer_buf(bus, tx, rx, (size_t)n);
    hal_gpio_set(cs);
    io_puts("tx: ");
    print_bytes(tx, (uint32_t)n);
    io_puts("rx: ");
    print_bytes(rx, (uint32_t)n);
    return CLI_OK;
}

static const cli_subcmd_t spi_subs[] = {
    { "init", sub_spi_init, "<1|2> [hz] [mode 0-3] [lsb]", "configure the bus" },
    { "xfer", sub_spi_xfer, "<bus> <cs-pin> <bytes...>",   "full duplex transfer" },
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------ UART ------------------------------ */

static bool parse_uart(const char *s, hal_uart_t *u)
{
    uint32_t n;
    if (!cli_arg_u32_range(s, 1U, HAL_UART_COUNT, &n)) {
        return false;
    }
    *u = (hal_uart_t)(n - 1U);
    return true;
}

static int sub_uart_init(int argc, char *argv[])
{
    hal_uart_t u;
    uint32_t   baud = 115200U;
    if ((argc < 2) || !parse_uart(argv[1], &u)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 300U, 4500000U, &baud)) {
        return CLI_ERR;
    }
    if (u == BOARD_CONSOLE_UART) {
        io_println("warning: this is the console UART");
    }
    return report(hal_uart_init(u, baud));
}

static int sub_uart_send(int argc, char *argv[])
{
    hal_uart_t u;
    if ((argc < 3) || !parse_uart(argv[1], &u)) {
        return CLI_ERR_USAGE;
    }
    for (int i = 2; i < argc; i++) {
        hal_uart_puts(u, argv[i]);
        hal_uart_putc(u, (i < (argc - 1)) ? ' ' : '\n');
    }
    return CLI_OK;
}

static int sub_uart_recv(int argc, char *argv[])
{
    hal_uart_t u;
    uint8_t    buf[16];
    uint32_t   n = 0U;
    if ((argc != 2) || !parse_uart(argv[1], &u)) {
        return CLI_ERR_USAGE;
    }
    int c;
    uint32_t total = 0U;
    while ((c = hal_uart_getc(u)) >= 0) {
        buf[n++] = (uint8_t)c;
        if (n == sizeof(buf)) {
            hexdump(buf, n, total);
            total += n;
            n = 0U;
        }
    }
    if (n > 0U) {
        hexdump(buf, n, total);
    }
    io_printf("%lu bytes, %lu dropped" IO_NEWLINE, (unsigned long)(total + n),
              (unsigned long)hal_uart_rx_dropped(u));
    return CLI_OK;
}

static int sub_uart_bridge(int argc, char *argv[])
{
    hal_uart_t u;
    if ((argc != 2) || !parse_uart(argv[1], &u) || (u == BOARD_CONSOLE_UART)) {
        return CLI_ERR_USAGE;
    }
    io_println("console <-> uart bridge, Ctrl-] to quit");
    for (;;) {
        const int c = io_getc();
        if ((c == 0x1D) || (c == IO_CLOSED)) {
            break;
        }
        if (c >= 0) {
            hal_uart_putc(u, (char)c);
        }
        const int r = hal_uart_getc(u);
        if (r >= 0) {
            io_putc((char)r);
        }
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

static const cli_subcmd_t uart_subs[] = {
    { "init",   sub_uart_init,   "<1-3> [baud]",      "configure a UART" },
    { "send",   sub_uart_send,   "<uart> <text...>",  "transmit text + newline" },
    { "recv",   sub_uart_recv,   "<uart>",            "hexdump what was received" },
    { "bridge", sub_uart_bridge, "<uart>",            "transparent console bridge (GPS, ESP...)" },
    { NULL, NULL, NULL, NULL }
};

static const cli_command_t bus_cmds[] = {
    { "i2c",  NULL, "<sub> ...", "I2C master tools",   i2c_subs },
    { "spi",  NULL, "<sub> ...", "SPI master tools",   spi_subs },
    { "uart", NULL, "<sub> ...", "extra serial ports", uart_subs },
};

static const cli_group_t bus_group = { "buses", bus_cmds, sizeof(bus_cmds) / sizeof(bus_cmds[0]) };

const cli_group_t *cmd_bus_group(void)
{
    return &bus_group;
}

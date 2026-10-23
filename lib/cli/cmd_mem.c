/**
 * @file    cmd_mem.c
 * @brief   Memory and register commands, for exploring the chip from
 *          the terminal like a debugger:
 *
 *   peek 0x4001100C            read GPIOC_ODR
 *   poke 0x40011010 0x100      write GPIOC_BSRR (set PC8)
 *   dump 0x08000000 64         hexdump of the vector table
 *   reg GPIOC                  decode the registers of a peripheral
 *   bit 0x4001100C 8 1         read-modify-write one bit
 *   field 0x40011004 0 4 0x2   write a bit field (CRH, pin 8 mode)
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "bits.h"
#include "hexdump.h"
#include "strutil.h"

#define DUMP_MAX_BYTES  (1024U)

/* A few register maps, so "reg GPIOC" prints names instead of numbers. */
typedef struct {
    const char *name;
    uint8_t     offset;
} reg_desc_t;

typedef struct {
    const char       *name;
    uint32_t          base;
    const reg_desc_t *regs;
    uint8_t           nregs;
} periph_desc_t;

static const reg_desc_t gpio_regs[] = {
    { "CRL", 0x00U }, { "CRH", 0x04U }, { "IDR", 0x08U }, { "ODR", 0x0CU }, { "LCKR", 0x18U }
};
static const reg_desc_t rcc_regs[] = {
    { "CR", 0x00U }, { "CFGR", 0x04U }, { "CIR", 0x08U }, { "APB2RSTR", 0x0CU },
    { "APB1RSTR", 0x10U }, { "AHBENR", 0x14U }, { "APB2ENR", 0x18U }, { "APB1ENR", 0x1CU },
    { "BDCR", 0x20U }, { "CSR", 0x24U }
};
static const reg_desc_t tim_regs[] = {
    { "CR1", 0x00U }, { "CR2", 0x04U }, { "DIER", 0x0CU }, { "SR", 0x10U }, { "CCMR1", 0x18U },
    { "CCMR2", 0x1CU }, { "CCER", 0x20U }, { "CNT", 0x24U }, { "PSC", 0x28U }, { "ARR", 0x2CU },
    { "CCR1", 0x34U }, { "CCR2", 0x38U }, { "CCR3", 0x3CU }, { "CCR4", 0x40U }
};
static const reg_desc_t usart_regs[] = {
    { "SR", 0x00U }, { "DR", 0x04U }, { "BRR", 0x08U }, { "CR1", 0x0CU }, { "CR2", 0x10U }, { "CR3", 0x14U }
};
static const reg_desc_t systick_regs[] = {
    { "CSR", 0x00U }, { "RVR", 0x04U }, { "CVR", 0x08U }, { "CALIB", 0x0CU }
};
static const reg_desc_t adc_regs[] = {
    { "SR", 0x00U }, { "CR1", 0x04U }, { "CR2", 0x08U }, { "SMPR1", 0x0CU }, { "SMPR2", 0x10U },
    { "SQR1", 0x2CU }, { "SQR3", 0x34U }, { "DR", 0x4CU }
};

#define REGS(r)  (r), (uint8_t)(sizeof(r) / sizeof((r)[0]))

static const periph_desc_t periphs[] = {
    { "GPIOA",   0x40010800UL, REGS(gpio_regs) },
    { "GPIOB",   0x40010C00UL, REGS(gpio_regs) },
    { "GPIOC",   0x40011000UL, REGS(gpio_regs) },
    { "GPIOD",   0x40011400UL, REGS(gpio_regs) },
    { "RCC",     0x40021000UL, REGS(rcc_regs) },
    { "TIM2",    0x40000000UL, REGS(tim_regs) },
    { "TIM3",    0x40000400UL, REGS(tim_regs) },
    { "TIM4",    0x40000800UL, REGS(tim_regs) },
    { "USART1",  0x40013800UL, REGS(usart_regs) },
    { "USART2",  0x40004400UL, REGS(usart_regs) },
    { "ADC1",    0x40012400UL, REGS(adc_regs) },
    { "SYSTICK", 0xE000E010UL, REGS(systick_regs) },
};

static bool parse_addr(const char *s, uint32_t *addr)
{
    /* accept a peripheral name too: "GPIOC+0x0C" */
    for (uint32_t i = 0U; i < (sizeof(periphs) / sizeof(periphs[0])); i++) {
        const size_t n = str_len(periphs[i].name);
        if ((str_ncmp(s, periphs[i].name, n) == 0) || (str_ncmp(s, str_to_lower((char[]){0}), 0) != 0)) {
            /* exact prefix (case sensitive), handled below */
        }
        char upper[16];
        (void)str_copy(upper, s, sizeof(upper));
        (void)str_to_upper(upper);
        if (str_ncmp(upper, periphs[i].name, n) == 0) {
            uint32_t off = 0U;
            if ((upper[n] == '+') && !str_to_u32(&s[n + 1U], &off)) {
                break;
            }
            if ((upper[n] != '\0') && (upper[n] != '+')) {
                continue;
            }
            *addr = periphs[i].base + off;
            return true;
        }
    }
    return cli_arg_u32(s, addr);
}

static int cmd_peek(int argc, char *argv[])
{
    uint32_t addr;
    uint32_t value;
    uint32_t count = 1U;
    if ((argc < 2) || !parse_addr(argv[1], &addr)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 1U, 64U, &count)) {
        return CLI_ERR;
    }
    for (uint32_t i = 0U; i < count; i++) {
        const uint32_t a = addr + (i * 4U);
        if (!hal_sys_mem_read32(a, &value)) {
            return cli_error("0x%08lX not readable (unaligned or outside memory map)", (unsigned long)a);
        }
        io_printf("0x%08lX: 0x%08lX  %10lu  ", (unsigned long)a, (unsigned long)value, (unsigned long)value);
        io_print_bin(value, 32U);
        io_puts(IO_NEWLINE);
    }
    return CLI_OK;
}

static int cmd_poke(int argc, char *argv[])
{
    uint32_t addr;
    uint32_t value;
    if ((argc != 3) || !parse_addr(argv[1], &addr) || !cli_arg_u32(argv[2], &value)) {
        return CLI_ERR_USAGE;
    }
    if (!hal_sys_mem_write32(addr, value)) {
        return cli_error("0x%08lX not writable", (unsigned long)addr);
    }
    return CLI_OK;
}

static int cmd_dump(int argc, char *argv[])
{
    uint8_t  buf[16];
    uint32_t addr;
    uint32_t len = 64U;
    if ((argc < 2) || !parse_addr(argv[1], &addr)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 1U, DUMP_MAX_BYTES, &len)) {
        return CLI_ERR;
    }
    for (uint32_t off = 0U; off < len; off += sizeof(buf)) {
        const uint32_t n = ((len - off) < sizeof(buf)) ? (len - off) : (uint32_t)sizeof(buf);
        for (uint32_t i = 0U; i < n; i++) {
            if (!hal_sys_mem_read8(addr + off + i, &buf[i])) {
                return cli_error("0x%08lX not readable", (unsigned long)(addr + off + i));
            }
        }
        hexdump(buf, n, addr + off);
    }
    return CLI_OK;
}

static int cmd_reg(int argc, char *argv[])
{
    if (argc != 2) {
        io_puts("peripherals:");
        for (uint32_t i = 0U; i < (sizeof(periphs) / sizeof(periphs[0])); i++) {
            io_printf(" %s", periphs[i].name);
        }
        io_puts(IO_NEWLINE);
        return (argc == 1) ? CLI_OK : CLI_ERR_USAGE;
    }
    for (uint32_t i = 0U; i < (sizeof(periphs) / sizeof(periphs[0])); i++) {
        if (str_ieq(argv[1], periphs[i].name)) {
            const periph_desc_t *p = &periphs[i];
            for (uint8_t r = 0U; r < p->nregs; r++) {
                uint32_t v;
                const uint32_t a = p->base + p->regs[r].offset;
                if (hal_sys_mem_read32(a, &v)) {
                    io_printf("%-8s %-9s 0x%08lX  0x%08lX" IO_NEWLINE, p->name, p->regs[r].name,
                              (unsigned long)a, (unsigned long)v);
                } else {
                    io_printf("%-8s %-9s 0x%08lX  (not accessible)" IO_NEWLINE, p->name, p->regs[r].name,
                              (unsigned long)a);
                }
            }
            return CLI_OK;
        }
    }
    return cli_error("unknown peripheral '%s' (type 'reg')", argv[1]);
}

static int cmd_bit(int argc, char *argv[])
{
    uint32_t addr;
    uint32_t bit;
    uint32_t value;
    if ((argc < 3) || !parse_addr(argv[1], &addr) || !cli_arg_u32_range(argv[2], 0U, 31U, &bit)) {
        return CLI_ERR_USAGE;
    }
    if (!hal_sys_mem_read32(addr, &value)) {
        return cli_error("not readable");
    }
    if (argc == 3) {
        io_printf("bit %lu = %lu" IO_NEWLINE, (unsigned long)bit, (unsigned long)((value >> bit) & 1U));
        return CLI_OK;
    }
    bool level;
    if (!cli_arg_bool(argv[3], &level)) {
        return CLI_ERR;
    }
    if (level) {
        BIT_SET(value, BIT(bit));
    } else {
        BIT_CLR(value, BIT(bit));
    }
    return hal_sys_mem_write32(addr, value) ? CLI_OK : cli_error("not writable");
}

static int cmd_field(int argc, char *argv[])
{
    uint32_t addr;
    uint32_t pos;
    uint32_t width;
    uint32_t value;
    if ((argc < 4) || !parse_addr(argv[1], &addr) ||
        !cli_arg_u32_range(argv[2], 0U, 31U, &pos) || !cli_arg_u32_range(argv[3], 1U, 32U - pos, &width)) {
        return CLI_ERR_USAGE;
    }
    if (!hal_sys_mem_read32(addr, &value)) {
        return cli_error("not readable");
    }
    if (argc == 4) {
        io_printf("field [%lu:%lu] = 0x%lX" IO_NEWLINE, (unsigned long)(pos + width - 1U), (unsigned long)pos,
                  (unsigned long)FIELD_GET(value, pos, width));
        return CLI_OK;
    }
    uint32_t field;
    if (!cli_arg_u32_range(argv[4], 0U, BIT_MASK(width), &field)) {
        return CLI_ERR;
    }
    FIELD_SET(value, pos, width, field);
    return hal_sys_mem_write32(addr, value) ? CLI_OK : cli_error("not writable");
}

static const cli_command_t mem_cmds[] = {
    { "peek",  cmd_peek,  "<addr|PERIPH+off> [n]",         "read 32-bit words (hex/dec/bin)", NULL },
    { "poke",  cmd_poke,  "<addr> <value>",                "write a 32-bit word",             NULL },
    { "dump",  cmd_dump,  "<addr> [len]",                  "hexdump memory",                  NULL },
    { "reg",   cmd_reg,   "[GPIOC|RCC|TIM3|USART1|ADC1|SYSTICK...]", "dump peripheral registers", NULL },
    { "bit",   cmd_bit,   "<addr> <bit> [0|1]",            "read or write one bit",           NULL },
    { "field", cmd_field, "<addr> <pos> <width> [value]",  "read or write a bit field",       NULL },
};

static const cli_group_t mem_group = { "memory", mem_cmds, sizeof(mem_cmds) / sizeof(mem_cmds[0]) };

const cli_group_t *cmd_mem_group(void)
{
    return &mem_group;
}

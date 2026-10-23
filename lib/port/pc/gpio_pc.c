/**
 * @file    gpio_pc.c
 * @brief   Simulated GPIO ports with the F1 register model.
 *
 * Each port keeps an ODR, a mode per pin, and what the "outside world"
 * drives on input pins. IDR is computed like the hardware does:
 * outputs read back their ODR bit, inputs read the external level,
 * or their pull resistor when nothing drives them.
 * CRL/CRH/IDR/ODR/BSRR/BRR are visible at the real F1 addresses
 * through hal_sys_mem_read32 (CLI "peek 0x4001100C" = GPIOC_ODR).
 */
#include "sim_internal.h"
#include "board.h"

#define GPIO_REG_BASE    (0x40010800U)  /* GPIOA on the F1 */
#define GPIO_PORT_STRIDE (0x400U)
#define GPIO_OFF_CRL     (0x00U)
#define GPIO_OFF_CRH     (0x04U)
#define GPIO_OFF_IDR     (0x08U)
#define GPIO_OFF_ODR     (0x0CU)
#define GPIO_OFF_BSRR    (0x10U)
#define GPIO_OFF_BRR     (0x14U)

typedef struct {
    uint16_t odr;
    uint16_t driven_mask;     /* pins driven from outside           */
    uint16_t driven_level;
    uint8_t  mode[16];
} sim_port_t;

static sim_port_t s_port[HAL_PORT_COUNT];
static bool       s_show_leds;

static bool mode_is_output(uint8_t mode)
{
    return (mode == HAL_GPIO_OUTPUT) || (mode == HAL_GPIO_OUTPUT_FAST) ||
           (mode == HAL_GPIO_OUTPUT_OD) || (mode == HAL_GPIO_AF) || (mode == HAL_GPIO_AF_OD);
}

/* What IDR would read right now. */
static uint16_t compute_idr(const sim_port_t *p)
{
    uint16_t idr = 0U;
    for (uint32_t pin = 0U; pin < 16U; pin++) {
        const uint16_t m = (uint16_t)(1U << pin);
        bool level;
        if (mode_is_output(p->mode[pin]) && (p->mode[pin] != HAL_GPIO_OUTPUT_OD)) {
            level = (p->odr & m) != 0U;
        } else if ((p->driven_mask & m) != 0U) {
            /* open drain: line low if we pull low OR the outside does */
            level = ((p->driven_level & m) != 0U) &&
                    ((p->mode[pin] != HAL_GPIO_OUTPUT_OD) || ((p->odr & m) != 0U));
        } else if (p->mode[pin] == HAL_GPIO_OUTPUT_OD) {
            level = (p->odr & m) != 0U;      /* assume an external pull-up */
        } else if (p->mode[pin] == HAL_GPIO_INPUT_PULLUP) {
            level = true;
        } else {
            level = false;                   /* floating / pull-down / analog */
        }
        if (level) {
            idr |= m;
        }
    }
    return idr;
}

static void show_leds(void)
{
    static const hal_pin_t leds[] = BOARD_LEDS_INIT;
    char line[64];
    size_t n = 0U;

    line[n++] = '[';
    for (size_t i = 0U; i < (sizeof(leds) / sizeof(leds[0])); i++) {
        const bool on = ((s_port[HAL_PIN_PORT(leds[i])].odr & HAL_PIN_MASK(leds[i])) != 0U)
                        == (BOARD_LED_ACTIVE_HIGH != 0);
        line[n++] = on ? '#' : '.';
    }
    line[n++] = ']';
    line[n]   = '\0';
    sim_printf("\x1b[s\x1b[1;70H LEDs %s \x1b[u", line);  /* top right corner */
}

/* Apply a change and report the edges it caused. */
static void port_update(hal_port_t port, uint16_t new_odr, uint16_t new_driven_mask,
                        uint16_t new_driven_level)
{
    sim_port_t    *p        = &s_port[port];
    const uint32_t irq      = hal_irq_save();
    const uint16_t old_idr  = compute_idr(p);
    const uint16_t old_odr  = p->odr;

    p->odr          = new_odr;
    p->driven_mask  = new_driven_mask;
    p->driven_level = new_driven_level;
    const uint16_t new_idr = compute_idr(p);
    hal_irq_restore(irq);

    const uint16_t odr_changed = (uint16_t)(old_odr ^ new_odr);
    const uint16_t idr_changed = (uint16_t)(old_idr ^ new_idr);
    for (uint32_t pin = 0U; pin < 16U; pin++) {
        const uint16_t  m  = (uint16_t)(1U << pin);
        const hal_pin_t hp = HAL_PIN(port, pin);
        if ((odr_changed & m) != 0U) {
            char name[6];
            sim_log(SIM_LOG_GPIO, "%s -> %u", hal_gpio_name(hp, name), (new_odr & m) ? 1U : 0U);
            sim_spi_on_pin_change(hp, (new_odr & m) != 0U);
        }
        if ((idr_changed & m) != 0U) {
            sim_exti_on_input_change(hp, (old_idr & m) != 0U, (new_idr & m) != 0U);
        }
    }
    if (s_show_leds && (odr_changed != 0U)) {
        show_leds();
    }
}

/* ------------------------------ HAL API --------------------------- */

void hal_gpio_port_mode(hal_port_t port, uint16_t mask, hal_gpio_mode_t mode)
{
    if ((port >= HAL_PORT_COUNT) || (mask == 0U) || ((uint32_t)mode >= HAL_GPIO_MODE_COUNT)) {
        return;
    }
    sim_core_ensure_started();
    sim_port_t *p   = &s_port[port];
    uint16_t    odr = p->odr;

    for (uint32_t pin = 0U; pin < 16U; pin++) {
        if ((mask & (1U << pin)) != 0U) {
            p->mode[pin] = (uint8_t)mode;
        }
    }
    /* F1: pull direction lives in ODR */
    if (mode == HAL_GPIO_INPUT_PULLUP) {
        odr |= mask;
    } else if (mode == HAL_GPIO_INPUT_PULLDOWN) {
        odr &= (uint16_t)~mask;
    } else {
        /* unchanged */
    }
    {
        char name[6];
        for (uint32_t pin = 0U; pin < 16U; pin++) {
            if ((mask & (1U << pin)) != 0U) {
                sim_log(SIM_LOG_GPIO, "%s mode %s", hal_gpio_name(HAL_PIN(port, pin), name),
                        hal_gpio_mode_name(mode));
            }
        }
    }
    port_update(port, odr, p->driven_mask, p->driven_level);
}

void hal_gpio_mode(hal_pin_t pin, hal_gpio_mode_t mode)
{
    hal_gpio_port_mode(HAL_PIN_PORT(pin), HAL_PIN_MASK(pin), mode);
}

hal_gpio_mode_t hal_gpio_get_mode(hal_pin_t pin)
{
    return hal_gpio_pin_valid(pin) ? (hal_gpio_mode_t)s_port[HAL_PIN_PORT(pin)].mode[HAL_PIN_NUM(pin)]
                                   : HAL_GPIO_INPUT;
}

void hal_gpio_port_write(hal_port_t port, uint16_t set_mask, uint16_t clr_mask)
{
    if (port >= HAL_PORT_COUNT) {
        return;
    }
    const sim_port_t *p = &s_port[port];
    /* BSRR semantics: set wins over reset */
    const uint16_t odr = (uint16_t)((p->odr & (uint16_t)~clr_mask) | set_mask);
    port_update(port, odr, p->driven_mask, p->driven_level);
}

void hal_gpio_port_toggle(hal_port_t port, uint16_t mask)
{
    if (port < HAL_PORT_COUNT) {
        const sim_port_t *p = &s_port[port];
        port_update(port, (uint16_t)(p->odr ^ mask), p->driven_mask, p->driven_level);
    }
}

void hal_gpio_set(hal_pin_t pin)
{
    hal_gpio_port_write(HAL_PIN_PORT(pin), HAL_PIN_MASK(pin), 0U);
}

void hal_gpio_clear(hal_pin_t pin)
{
    hal_gpio_port_write(HAL_PIN_PORT(pin), 0U, HAL_PIN_MASK(pin));
}

void hal_gpio_write(hal_pin_t pin, bool level)
{
    if (level) {
        hal_gpio_set(pin);
    } else {
        hal_gpio_clear(pin);
    }
}

void hal_gpio_toggle(hal_pin_t pin)
{
    hal_gpio_port_toggle(HAL_PIN_PORT(pin), HAL_PIN_MASK(pin));
}

uint16_t hal_gpio_port_read(hal_port_t port)
{
    return (port < HAL_PORT_COUNT) ? compute_idr(&s_port[port]) : 0U;
}

uint16_t hal_gpio_port_read_output(hal_port_t port)
{
    return (port < HAL_PORT_COUNT) ? s_port[port].odr : 0U;
}

bool hal_gpio_read(hal_pin_t pin)
{
    return hal_gpio_pin_valid(pin) && ((hal_gpio_port_read(HAL_PIN_PORT(pin)) & HAL_PIN_MASK(pin)) != 0U);
}

bool hal_gpio_read_output(hal_pin_t pin)
{
    return hal_gpio_pin_valid(pin) && ((s_port[HAL_PIN_PORT(pin)].odr & HAL_PIN_MASK(pin)) != 0U);
}

/* ------------------------------ sim API --------------------------- */

void sim_gpio_input(hal_pin_t pin, bool level)
{
    if (!hal_gpio_pin_valid(pin)) {
        return;
    }
    const sim_port_t *p = &s_port[HAL_PIN_PORT(pin)];
    const uint16_t    m = HAL_PIN_MASK(pin);
    char name[6];
    sim_log(SIM_LOG_GPIO, "%s driven %u", hal_gpio_name(pin, name), level ? 1U : 0U);
    port_update(HAL_PIN_PORT(pin), p->odr, (uint16_t)(p->driven_mask | m),
                level ? (uint16_t)(p->driven_level | m) : (uint16_t)(p->driven_level & (uint16_t)~m));
}

void sim_gpio_release(hal_pin_t pin)
{
    if (!hal_gpio_pin_valid(pin)) {
        return;
    }
    const sim_port_t *p = &s_port[HAL_PIN_PORT(pin)];
    port_update(HAL_PIN_PORT(pin), p->odr, (uint16_t)(p->driven_mask & (uint16_t)~HAL_PIN_MASK(pin)),
                p->driven_level);
}

void sim_gpio_show_leds(bool enable)
{
    s_show_leds = enable;
    if (enable) {
        show_leds();
    }
}

/* -------------------------- register model ------------------------ */

/* F1 CNF[1:0]MODE[1:0] nibble for a HAL mode (RM0008 table 20). */
static uint32_t mode_to_nibble(uint8_t mode)
{
    switch (mode) {
    case HAL_GPIO_ANALOG:         return 0x0U;
    case HAL_GPIO_INPUT:          return 0x4U;
    case HAL_GPIO_INPUT_PULLUP:
    case HAL_GPIO_INPUT_PULLDOWN: return 0x8U;
    case HAL_GPIO_OUTPUT:         return 0x2U;
    case HAL_GPIO_OUTPUT_FAST:    return 0x3U;
    case HAL_GPIO_OUTPUT_OD:      return 0x6U;
    case HAL_GPIO_AF:             return 0xBU;
    case HAL_GPIO_AF_OD:          return 0xFU;
    default:                      return 0x4U;
    }
}

static hal_gpio_mode_t nibble_to_mode(uint32_t nib, bool odr_bit)
{
    const uint32_t mode = nib & 0x3U;
    const uint32_t cnf  = (nib >> 2) & 0x3U;
    if (mode == 0U) {
        static const hal_gpio_mode_t in[4] = {
            HAL_GPIO_ANALOG, HAL_GPIO_INPUT, HAL_GPIO_INPUT_PULLDOWN, HAL_GPIO_INPUT
        };
        if (cnf == 2U) {
            return odr_bit ? HAL_GPIO_INPUT_PULLUP : HAL_GPIO_INPUT_PULLDOWN;
        }
        return in[cnf];
    }
    static const hal_gpio_mode_t out[4] = {
        HAL_GPIO_OUTPUT, HAL_GPIO_OUTPUT_OD, HAL_GPIO_AF, HAL_GPIO_AF_OD
    };
    if ((cnf == 0U) && (mode == 3U)) {
        return HAL_GPIO_OUTPUT_FAST;
    }
    return out[cnf];
}

static bool decode_addr(uint32_t addr, hal_port_t *port, uint32_t *offset)
{
    if ((addr < GPIO_REG_BASE) || ((addr & 3U) != 0U)) {
        return false;
    }
    const uint32_t idx = (addr - GPIO_REG_BASE) / GPIO_PORT_STRIDE;
    const uint32_t off = (addr - GPIO_REG_BASE) % GPIO_PORT_STRIDE;
    if ((idx >= HAL_PORT_COUNT) || (off > GPIO_OFF_BRR)) {
        return false;
    }
    *port   = (hal_port_t)idx;
    *offset = off;
    return true;
}

bool sim_gpio_reg_read(uint32_t addr, uint32_t *value)
{
    hal_port_t port;
    uint32_t   off;
    if (!decode_addr(addr, &port, &off)) {
        return false;
    }
    const sim_port_t *p = &s_port[port];
    uint32_t v = 0U;
    switch (off) {
    case GPIO_OFF_CRL:
    case GPIO_OFF_CRH: {
        const uint32_t first = (off == GPIO_OFF_CRL) ? 0U : 8U;
        for (uint32_t i = 0U; i < 8U; i++) {
            v |= mode_to_nibble(p->mode[first + i]) << (i * 4U);
        }
        break;
    }
    case GPIO_OFF_IDR: v = compute_idr(p); break;
    case GPIO_OFF_ODR: v = p->odr;         break;
    default:           v = 0U;             break;   /* BSRR/BRR read 0 */
    }
    *value = v;
    return true;
}

bool sim_gpio_reg_write(uint32_t addr, uint32_t value)
{
    hal_port_t port;
    uint32_t   off;
    if (!decode_addr(addr, &port, &off)) {
        return false;
    }
    const sim_port_t *p = &s_port[port];
    switch (off) {
    case GPIO_OFF_CRL:
    case GPIO_OFF_CRH: {
        const uint32_t first = (off == GPIO_OFF_CRL) ? 0U : 8U;
        for (uint32_t i = 0U; i < 8U; i++) {
            const uint32_t pin = first + i;
            const hal_gpio_mode_t m = nibble_to_mode((value >> (i * 4U)) & 0xFU,
                                                     (p->odr & (1U << pin)) != 0U);
            s_port[port].mode[pin] = (uint8_t)m;
        }
        port_update(port, p->odr, p->driven_mask, p->driven_level);
        break;
    }
    case GPIO_OFF_ODR:
        port_update(port, (uint16_t)value, p->driven_mask, p->driven_level);
        break;
    case GPIO_OFF_BSRR:
        hal_gpio_port_write(port, (uint16_t)value, (uint16_t)(value >> 16));
        break;
    case GPIO_OFF_BRR:
        hal_gpio_port_write(port, 0U, (uint16_t)value);
        break;
    default:
        return false;       /* IDR is read-only */
    }
    return true;
}

void sim_gpio_status(void)
{
    sim_printf("GPIO   ODR    IDR    driven  (mode of used pins)\r\n");
    for (uint32_t port = 0U; port < HAL_PORT_COUNT; port++) {
        const sim_port_t *p = &s_port[port];
        sim_printf("  %c   %04X   %04X   %04X   ", (char)('A' + port), p->odr,
                   compute_idr(p), p->driven_mask);
        for (uint32_t pin = 0U; pin < 16U; pin++) {
            if (p->mode[pin] != (uint8_t)HAL_GPIO_INPUT) {
                sim_printf("%u:%s ", (unsigned)pin, hal_gpio_mode_name((hal_gpio_mode_t)p->mode[pin]));
            }
        }
        sim_printf("\r\n");
    }
}

/**
 * @file    hal_common.c
 * @brief   Port-independent parts of the HAL: names, parsers and the
 *          arithmetic used by every port (so it is tested once on PC).
 */
#include "hal.h"
#include "strutil.h"

/* ------------------------------ status ---------------------------- */

const char *hal_status_str(hal_status_t st)
{
    switch (st) {
    case HAL_OK:           return "ok";
    case HAL_ERROR:        return "error";
    case HAL_EINVAL:       return "invalid argument";
    case HAL_ETIMEOUT:     return "timeout";
    case HAL_ENACK:        return "no ack";
    case HAL_EBUSY:        return "busy";
    case HAL_EUNSUPPORTED: return "unsupported";
    default:               return "?";
    }
}

/* ------------------------------- GPIO ----------------------------- */

static const char *const gpio_mode_names[HAL_GPIO_MODE_COUNT] = {
    "in", "pullup", "pulldown", "analog", "out", "outfast", "od", "af", "afod"
};

bool hal_gpio_pin_valid(hal_pin_t pin)
{
    return HAL_PIN_PORT(pin) < HAL_PORT_COUNT;
}

bool hal_gpio_parse(const char *name, hal_pin_t *pin)
{
    uint32_t num;

    if ((name[0] == 'P') || (name[0] == 'p')) {
        name++;
    }
    const char port_letter = chr_to_upper(name[0]);
    if ((port_letter < 'A') || (port_letter >= (char)('A' + HAL_PORT_COUNT))) {
        return false;
    }
    if (!str_to_u32(&name[1], &num) || (num > 15U)) {
        return false;
    }
    *pin = HAL_PIN((uint32_t)(port_letter - 'A'), num);
    return true;
}

const char *hal_gpio_name(hal_pin_t pin, char *buf)
{
    const uint32_t num = HAL_PIN_NUM(pin);
    if (!hal_gpio_pin_valid(pin)) {
        buf[0] = '-';
        buf[1] = '\0';
        return buf;
    }
    buf[0] = 'P';
    buf[1] = (char)('A' + (char)HAL_PIN_PORT(pin));
    if (num >= 10U) {
        buf[2] = '1';
        buf[3] = (char)('0' + (char)(num - 10U));
        buf[4] = '\0';
    } else {
        buf[2] = (char)('0' + (char)num);
        buf[3] = '\0';
    }
    return buf;
}

const char *hal_gpio_mode_name(hal_gpio_mode_t mode)
{
    return ((uint32_t)mode < HAL_GPIO_MODE_COUNT) ? gpio_mode_names[mode] : "?";
}

bool hal_gpio_parse_mode(const char *name, hal_gpio_mode_t *mode)
{
    for (uint32_t i = 0U; i < HAL_GPIO_MODE_COUNT; i++) {
        if (str_ieq(name, gpio_mode_names[i])) {
            *mode = (hal_gpio_mode_t)i;
            return true;
        }
    }
    /* a few friendly aliases */
    if (str_ieq(name, "input"))  { *mode = HAL_GPIO_INPUT;  return true; }
    if (str_ieq(name, "output")) { *mode = HAL_GPIO_OUTPUT; return true; }
    return false;
}

/* ------------------------------ timers ---------------------------- */

#define HAL_TIMER_CALC_TRIES  (512U)   /* bounded search for an exact PSC */

bool hal_timer_calc(uint32_t clk_hz, uint32_t freq_hz, uint16_t *psc, uint16_t *arr)
{
    if ((freq_hz == 0U) || (freq_hz > (clk_hz / 2U))) {
        return false;
    }
    /* total timer ticks per period, rounded to nearest */
    const uint32_t total = (clk_hz + (freq_hz / 2U)) / freq_hz;
    /* smallest prescaler that lets ARR fit in 16 bits */
    const uint32_t p = (total - 1U) / 65536U;
    if (p > 0xFFFFU) {
        return false;                       /* frequency too low */
    }
    /* prefer an exact split total = (PSC+1)*(ARR+1): try a few larger
     * prescalers that divide total evenly (72 MHz / 1 Hz -> 1125 * 64000) */
    for (uint32_t cand = p; (cand <= 0xFFFFU) && (cand < (p + HAL_TIMER_CALC_TRIES)); cand++) {
        if ((total % (cand + 1U)) == 0U) {
            *psc = (uint16_t)cand;
            *arr = (uint16_t)((total / (cand + 1U)) - 1U);
            return true;
        }
    }
    const uint32_t a = ((total + ((p + 1U) / 2U)) / (p + 1U)) - 1U;
    *psc = (uint16_t)p;
    *arr = (uint16_t)((a > 0xFFFFU) ? 0xFFFFU : a);
    return true;
}

/* ------------------------------- SPI ------------------------------ */

uint32_t hal_spi_calc_div_log2(uint32_t pclk_hz, uint32_t max_hz)
{
    for (uint32_t k = 1U; k <= 8U; k++) {
        if ((pclk_hz >> k) <= max_hz) {
            return k;
        }
    }
    return 0U;
}

/* ------------------------------- ADC ------------------------------ */

#define ADC_TEMP_V25_MV         (1430)   /* datasheet typical   */
#define ADC_TEMP_SLOPE_UV_PER_C (4300)   /* 4.3 mV / degree     */

uint32_t hal_adc_raw_to_mv(uint32_t raw, uint32_t vdda_mv)
{
    return ((raw * vdda_mv) + (HAL_ADC_MAX / 2U)) / HAL_ADC_MAX;
}

int32_t hal_adc_mv_to_temp_c10(uint32_t sense_mv)
{
    /* T[0.1 C] = (V25 - V) * 10000 / slope_uV + 250 */
    const int32_t dv_uv10 = (ADC_TEMP_V25_MV - (int32_t)sense_mv) * 10000;
    return (dv_uv10 / ADC_TEMP_SLOPE_UV_PER_C) + 250;
}

/* ------------------------------- I2C ------------------------------ */

hal_status_t hal_i2c_reg_write8(hal_i2c_t bus, uint8_t addr, uint8_t reg, uint8_t value)
{
    const uint8_t buf[2] = { reg, value };
    return hal_i2c_write(bus, addr, buf, sizeof(buf));
}

hal_status_t hal_i2c_reg_read8(hal_i2c_t bus, uint8_t addr, uint8_t reg, uint8_t *value)
{
    return hal_i2c_write_read(bus, addr, &reg, 1U, value, 1U);
}

hal_status_t hal_i2c_reg_read(hal_i2c_t bus, uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    return hal_i2c_write_read(bus, addr, &reg, 1U, data, len);
}

hal_status_t hal_i2c_reg_update8(hal_i2c_t bus, uint8_t addr, uint8_t reg,
                                 uint8_t mask, uint8_t value)
{
    uint8_t      old = 0U;
    hal_status_t st  = hal_i2c_reg_read8(bus, addr, reg, &old);
    if (st != HAL_OK) {
        return st;
    }
    const uint8_t updated = (uint8_t)((old & (uint8_t)~mask) | (value & mask));
    return (updated == old) ? HAL_OK : hal_i2c_reg_write8(bus, addr, reg, updated);
}

/* ------------------------------ console --------------------------- */

void hal_console_init(uint32_t baud)
{
    (void)hal_uart_init(BOARD_CONSOLE_UART, baud);
}

void hal_console_putc(char ch)
{
    hal_uart_putc(BOARD_CONSOLE_UART, ch);
}

int hal_console_getc(void)
{
    return hal_uart_getc(BOARD_CONSOLE_UART);
}

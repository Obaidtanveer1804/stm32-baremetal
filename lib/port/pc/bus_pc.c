/**
 * @file    bus_pc.c
 * @brief   Simulated SPI and I2C buses. Transfers are routed to the
 *          device models attached with sim_spi_attach / sim_i2c_attach;
 *          with no device, SPI reads 0xFF (MISO pull-up) and I2C NACKs,
 *          exactly what an empty breadboard does.
 */
#include "sim_internal.h"
#include "io.h"

static sim_spi_dev_t *s_spi_devs[HAL_SPI_COUNT];
static sim_i2c_dev_t *s_i2c_devs[HAL_I2C_COUNT];
static uint32_t       s_spi_hz[HAL_SPI_COUNT];
static bool           s_i2c_ready[HAL_I2C_COUNT];

#define SIM_SPI_PCLK_HZ  (24000000UL)

/* ------------------------------- SPI ------------------------------ */

void sim_spi_attach(hal_spi_t bus, sim_spi_dev_t *dev)
{
    if ((bus < HAL_SPI_COUNT) && (dev != NULL)) {
        dev->next       = s_spi_devs[bus];
        s_spi_devs[bus] = dev;
    }
}

hal_status_t hal_spi_init(hal_spi_t spi, const hal_spi_config_t *cfg)
{
    if ((spi >= HAL_SPI_COUNT) || (cfg == NULL)) {
        return HAL_EINVAL;
    }
    const uint32_t k = hal_spi_calc_div_log2(SIM_SPI_PCLK_HZ, cfg->max_hz);
    if (k == 0U) {
        return HAL_EINVAL;
    }
    s_spi_hz[spi] = SIM_SPI_PCLK_HZ >> k;
    sim_log(SIM_LOG_SPI, "spi%u init mode %u %s %lu Hz", (unsigned)spi + 1U, (unsigned)cfg->mode,
            cfg->lsb_first ? "lsb" : "msb", (unsigned long)s_spi_hz[spi]);
    return HAL_OK;
}

static sim_spi_dev_t *spi_selected(hal_spi_t spi)
{
    for (sim_spi_dev_t *d = s_spi_devs[spi]; d != NULL; d = d->next) {
        /* chip select is active low and must be an output */
        if (!hal_gpio_read_output(d->cs)) {
            return d;
        }
    }
    return NULL;
}

uint8_t hal_spi_transfer(hal_spi_t spi, uint8_t tx)
{
    if (spi >= HAL_SPI_COUNT) {
        return 0xFFU;
    }
    sim_spi_dev_t *dev = spi_selected(spi);
    const uint8_t  rx  = ((dev != NULL) && (dev->transfer != NULL)) ? dev->transfer(dev, tx) : 0xFFU;
    sim_log(SIM_LOG_SPI, "spi%u %s mosi %02X miso %02X", (unsigned)spi + 1U,
            (dev != NULL) ? dev->name : "(none)", (unsigned)tx, (unsigned)rx);
    return rx;
}

void hal_spi_transfer_buf(hal_spi_t spi, const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0U; i < len; i++) {
        const uint8_t in = hal_spi_transfer(spi, (tx != NULL) ? tx[i] : 0xFFU);
        if (rx != NULL) {
            rx[i] = in;
        }
    }
}

uint32_t hal_spi_get_hz(hal_spi_t spi)
{
    return (spi < HAL_SPI_COUNT) ? s_spi_hz[spi] : 0U;
}

void sim_spi_on_pin_change(hal_pin_t pin, bool level)
{
    for (uint32_t bus = 0U; bus < HAL_SPI_COUNT; bus++) {
        for (sim_spi_dev_t *d = s_spi_devs[bus]; d != NULL; d = d->next) {
            if ((d->cs == pin) && (d->select != NULL)) {
                d->select(d, !level);      /* CS low = selected */
            }
        }
    }
}

/* ------------------------------- I2C ------------------------------ */

void sim_i2c_attach(hal_i2c_t bus, sim_i2c_dev_t *dev)
{
    if ((bus < HAL_I2C_COUNT) && (dev != NULL)) {
        dev->next       = s_i2c_devs[bus];
        s_i2c_devs[bus] = dev;
    }
}

static sim_i2c_dev_t *i2c_find(hal_i2c_t bus, uint8_t addr)
{
    for (sim_i2c_dev_t *d = s_i2c_devs[bus]; d != NULL; d = d->next) {
        if (d->addr == addr) {
            return d;
        }
    }
    return NULL;
}

hal_status_t hal_i2c_init(hal_i2c_t bus, uint32_t speed_hz)
{
    if (bus >= HAL_I2C_COUNT) {
        return HAL_EINVAL;
    }
    s_i2c_ready[bus] = true;
    sim_log(SIM_LOG_I2C, "i2c%u init %lu Hz", (unsigned)bus + 1U, (unsigned long)speed_hz);
    return HAL_OK;
}

static hal_status_t i2c_write_phase(hal_i2c_t bus, sim_i2c_dev_t *d, const uint8_t *data, size_t len)
{
    if ((d->start != NULL) && !d->start(d, false)) {
        sim_log(SIM_LOG_I2C, "i2c%u %02X: address NACK (busy)", (unsigned)bus + 1U, (unsigned)d->addr);
        return HAL_ENACK;
    }
    for (size_t i = 0U; i < len; i++) {
        const bool ack = (d->write != NULL) && d->write(d, data[i]);
        sim_log(SIM_LOG_I2C, "i2c%u %02X w %02X %s", (unsigned)bus + 1U, (unsigned)d->addr,
                (unsigned)data[i], ack ? "ack" : "NACK");
        if (!ack) {
            return HAL_ENACK;
        }
    }
    return HAL_OK;
}

static hal_status_t i2c_read_phase(hal_i2c_t bus, sim_i2c_dev_t *d, uint8_t *data, size_t len)
{
    if ((d->start != NULL) && !d->start(d, true)) {
        sim_log(SIM_LOG_I2C, "i2c%u %02X: address NACK (busy)", (unsigned)bus + 1U, (unsigned)d->addr);
        return HAL_ENACK;
    }
    for (size_t i = 0U; i < len; i++) {
        data[i] = (d->read != NULL) ? d->read(d) : 0xFFU;
        sim_log(SIM_LOG_I2C, "i2c%u %02X r %02X", (unsigned)bus + 1U, (unsigned)d->addr,
                (unsigned)data[i]);
    }
    return HAL_OK;
}

static void i2c_stop(sim_i2c_dev_t *d)
{
    if (d->stop != NULL) {
        d->stop(d);
    }
}

static sim_i2c_dev_t *i2c_begin(hal_i2c_t bus, uint8_t addr, hal_status_t *st)
{
    if ((bus >= HAL_I2C_COUNT) || !s_i2c_ready[bus]) {
        *st = (bus >= HAL_I2C_COUNT) ? HAL_EINVAL : HAL_ERROR;
        return NULL;
    }
    sim_i2c_dev_t *d = i2c_find(bus, addr);
    if (d == NULL) {
        sim_log(SIM_LOG_I2C, "i2c%u %02X: no ack (nobody there)", (unsigned)bus + 1U, (unsigned)addr);
        *st = HAL_ENACK;
    } else {
        *st = HAL_OK;
    }
    return d;
}

hal_status_t hal_i2c_write(hal_i2c_t bus, uint8_t addr, const uint8_t *data, size_t len)
{
    hal_status_t   st;
    sim_i2c_dev_t *d = i2c_begin(bus, addr, &st);
    if (d == NULL) {
        return st;
    }
    st = i2c_write_phase(bus, d, data, len);
    i2c_stop(d);
    return st;
}

hal_status_t hal_i2c_read(hal_i2c_t bus, uint8_t addr, uint8_t *data, size_t len)
{
    hal_status_t   st;
    sim_i2c_dev_t *d = i2c_begin(bus, addr, &st);
    if (d == NULL) {
        return st;
    }
    st = i2c_read_phase(bus, d, data, len);
    i2c_stop(d);
    return st;
}

hal_status_t hal_i2c_write_read(hal_i2c_t bus, uint8_t addr,
                                const uint8_t *wdata, size_t wlen,
                                uint8_t *rdata, size_t rlen)
{
    hal_status_t   st;
    sim_i2c_dev_t *d = i2c_begin(bus, addr, &st);
    if (d == NULL) {
        return st;
    }
    st = i2c_write_phase(bus, d, wdata, wlen);
    if (st == HAL_OK) {
        st = i2c_read_phase(bus, d, rdata, rlen);   /* repeated start */
    }
    i2c_stop(d);
    return st;
}

bool hal_i2c_probe(hal_i2c_t bus, uint8_t addr)
{
    hal_status_t   st;
    sim_i2c_dev_t *d = i2c_begin(bus, addr, &st);
    bool           ack = false;
    if (d != NULL) {
        ack = (d->start == NULL) || d->start(d, false);
        i2c_stop(d);
    }
    return ack;
}

void sim_devices_list(void)
{
    char name[6];
    for (uint32_t bus = 0U; bus < HAL_I2C_COUNT; bus++) {
        for (const sim_i2c_dev_t *d = s_i2c_devs[bus]; d != NULL; d = d->next) {
            io_printf("  i2c%u 0x%02X  %s\r\n", (unsigned)bus + 1U, (unsigned)d->addr, d->name);
        }
    }
    for (uint32_t bus = 0U; bus < HAL_SPI_COUNT; bus++) {
        for (const sim_spi_dev_t *d = s_spi_devs[bus]; d != NULL; d = d->next) {
            io_printf("  spi%u cs=%s %s\r\n", (unsigned)bus + 1U, hal_gpio_name(d->cs, name), d->name);
        }
    }
}

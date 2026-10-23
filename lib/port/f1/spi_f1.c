/**
 * @file    spi_f1.c
 * @brief   hal_spi on SPI1 / SPI2, polled master, software chip select.
 */
#include "hal_spi.h"
#include "hal_gpio.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

typedef struct {
    uint32_t              base;
    enum rcc_periph_clken clock;
    enum rcc_periph_rst   reset;
    hal_pin_t             sck;
    hal_pin_t             miso;
    hal_pin_t             mosi;
    bool                  on_apb2;
} spi_hw_t;

static const spi_hw_t spi_hw[HAL_SPI_COUNT] = {
    { SPI1, RCC_SPI1, RST_SPI1, PA(5),  PA(6),  PA(7),  true  },
    { SPI2, RCC_SPI2, RST_SPI2, PB(13), PB(14), PB(15), false },
};

/* libopencm3 baud-rate field values for /2 ... /256 */
static const uint32_t spi_br[9] = {
    0U,
    SPI_CR1_BAUDRATE_FPCLK_DIV_2,  SPI_CR1_BAUDRATE_FPCLK_DIV_4,
    SPI_CR1_BAUDRATE_FPCLK_DIV_8,  SPI_CR1_BAUDRATE_FPCLK_DIV_16,
    SPI_CR1_BAUDRATE_FPCLK_DIV_32, SPI_CR1_BAUDRATE_FPCLK_DIV_64,
    SPI_CR1_BAUDRATE_FPCLK_DIV_128, SPI_CR1_BAUDRATE_FPCLK_DIV_256
};

static uint32_t s_hz[HAL_SPI_COUNT];

hal_status_t hal_spi_init(hal_spi_t spi, const hal_spi_config_t *cfg)
{
    if ((spi >= HAL_SPI_COUNT) || (cfg == NULL)) {
        return HAL_EINVAL;
    }
    const spi_hw_t *hw   = &spi_hw[spi];
    const uint32_t  pclk = hw->on_apb2 ? rcc_apb2_frequency : rcc_apb1_frequency;
    const uint32_t  k    = hal_spi_calc_div_log2(pclk, cfg->max_hz);
    if (k == 0U) {
        return HAL_EINVAL;
    }

    rcc_periph_clock_enable(hw->clock);
    rcc_periph_reset_pulse(hw->reset);
    hal_gpio_mode(hw->sck,  HAL_GPIO_AF);
    hal_gpio_mode(hw->mosi, HAL_GPIO_AF);
    hal_gpio_mode(hw->miso, HAL_GPIO_INPUT);

    const uint32_t cpol = ((cfg->mode == HAL_SPI_MODE2) || (cfg->mode == HAL_SPI_MODE3))
                        ? SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE : SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE;
    const uint32_t cpha = ((cfg->mode == HAL_SPI_MODE1) || (cfg->mode == HAL_SPI_MODE3))
                        ? SPI_CR1_CPHA_CLK_TRANSITION_2 : SPI_CR1_CPHA_CLK_TRANSITION_1;

    spi_init_master(hw->base, spi_br[k], cpol, cpha, SPI_CR1_DFF_8BIT,
                    cfg->lsb_first ? SPI_CR1_LSBFIRST : SPI_CR1_MSBFIRST);
    /* NSS managed by software and held high: the CS is a GPIO */
    spi_enable_software_slave_management(hw->base);
    spi_set_nss_high(hw->base);
    spi_enable(hw->base);

    s_hz[spi] = pclk >> k;
    return HAL_OK;
}

uint8_t hal_spi_transfer(hal_spi_t spi, uint8_t tx)
{
    if (spi >= HAL_SPI_COUNT) {
        return 0xFFU;
    }
    /* spi_xfer waits for TXE, writes DR, waits for RXNE, reads DR:
     * reading after every write avoids the overrun of the TP version */
    return (uint8_t)spi_xfer(spi_hw[spi].base, tx);
}

void hal_spi_transfer_buf(hal_spi_t spi, const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0U; i < len; i++) {
        const uint8_t out = (tx != NULL) ? tx[i] : 0xFFU;
        const uint8_t in  = hal_spi_transfer(spi, out);
        if (rx != NULL) {
            rx[i] = in;
        }
    }
}

uint32_t hal_spi_get_hz(hal_spi_t spi)
{
    return (spi < HAL_SPI_COUNT) ? s_hz[spi] : 0U;
}

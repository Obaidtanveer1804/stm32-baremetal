/**
 * @file    w25q.c
 * @brief   SPI NOR flash driver (see w25q.h).
 */
#include "w25q.h"
#include "hal_time.h"

#define CMD_WRITE_ENABLE   (0x06U)
#define CMD_READ_STATUS1   (0x05U)
#define CMD_READ_DATA      (0x03U)
#define CMD_PAGE_PROGRAM   (0x02U)
#define CMD_SECTOR_ERASE   (0x20U)
#define CMD_BLOCK_ERASE    (0xD8U)
#define CMD_CHIP_ERASE     (0xC7U)
#define CMD_JEDEC_ID       (0x9FU)
#define CMD_UNIQUE_ID      (0x4BU)
#define CMD_POWER_DOWN     (0xB9U)
#define CMD_RELEASE_PD     (0xABU)

#define SR1_BUSY           (1U << 0)
#define SR1_WEL            (1U << 1)

#define TIMEOUT_PAGE_MS    (5U)
#define TIMEOUT_SECTOR_MS  (500U)
#define TIMEOUT_BLOCK_MS   (2000U)
#define TIMEOUT_CHIP_MS    (60000U)

static void select(const w25q_t *f)   { hal_gpio_clear(f->cs); }
static void deselect(const w25q_t *f) { hal_gpio_set(f->cs); }

static void send_cmd_addr(const w25q_t *f, uint8_t cmd, uint32_t addr)
{
    const uint8_t hdr[4] = { cmd, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    hal_spi_transfer_buf(f->spi, hdr, NULL, sizeof(hdr));
}

static void simple_cmd(const w25q_t *f, uint8_t cmd)
{
    select(f);
    (void)hal_spi_transfer(f->spi, cmd);
    deselect(f);
}

bool w25q_is_busy(const w25q_t *f)
{
    select(f);
    (void)hal_spi_transfer(f->spi, CMD_READ_STATUS1);
    const uint8_t sr = hal_spi_transfer(f->spi, 0xFFU);
    deselect(f);
    return (sr & SR1_BUSY) != 0U;
}

hal_status_t w25q_wait(const w25q_t *f, uint32_t timeout_ms)
{
    const uint32_t t0 = hal_time_ms();
    while (w25q_is_busy(f)) {
        if ((hal_time_ms() - t0) > timeout_ms) {
            return HAL_ETIMEOUT;
        }
    }
    return HAL_OK;
}

hal_status_t w25q_init(w25q_t *f, hal_spi_t spi, hal_pin_t cs)
{
    f->spi = spi;
    f->cs  = cs;
    hal_gpio_set(cs);
    hal_gpio_mode(cs, HAL_GPIO_OUTPUT_FAST);
    w25q_wake(f);

    select(f);
    (void)hal_spi_transfer(spi, CMD_JEDEC_ID);
    hal_spi_transfer_buf(spi, NULL, f->jedec, sizeof(f->jedec));
    deselect(f);

    const uint8_t cap = f->jedec[2];
    if (((f->jedec[0] == 0x00U) && (cap == 0x00U)) || ((f->jedec[0] == 0xFFU) && (cap == 0xFFU))) {
        f->size = 0U;
        return HAL_ENACK;                 /* no chip on the bus */
    }
    /* capacity code n means 2^n bytes (0x15 -> 2 MiB) */
    f->size = ((cap >= 0x10U) && (cap <= 0x19U)) ? (1UL << cap) : 0U;
    return (f->size != 0U) ? HAL_OK : HAL_EUNSUPPORTED;
}

hal_status_t w25q_read(const w25q_t *f, uint32_t addr, void *data, size_t len)
{
    if ((addr + len) > f->size) {
        return HAL_EINVAL;
    }
    const hal_status_t st = w25q_wait(f, TIMEOUT_PAGE_MS);
    if (st != HAL_OK) {
        return st;
    }
    select(f);
    send_cmd_addr(f, CMD_READ_DATA, addr);
    hal_spi_transfer_buf(f->spi, NULL, (uint8_t *)data, len);   /* no length limit */
    deselect(f);
    return HAL_OK;
}

static hal_status_t write_enable(const w25q_t *f)
{
    simple_cmd(f, CMD_WRITE_ENABLE);
    return HAL_OK;
}

hal_status_t w25q_write(const w25q_t *f, uint32_t addr, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    if ((addr + len) > f->size) {
        return HAL_EINVAL;
    }
    while (len > 0U) {
        const size_t room  = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
        const size_t chunk = (len < room) ? len : room;
        hal_status_t st    = w25q_wait(f, TIMEOUT_PAGE_MS);
        if (st != HAL_OK) {
            return st;
        }
        (void)write_enable(f);
        select(f);
        send_cmd_addr(f, CMD_PAGE_PROGRAM, addr);
        hal_spi_transfer_buf(f->spi, p, NULL, chunk);
        deselect(f);                       /* programming starts here */
        p    += chunk;
        addr += (uint32_t)chunk;
        len  -= chunk;
    }
    return w25q_wait(f, TIMEOUT_PAGE_MS);
}

static hal_status_t erase(const w25q_t *f, uint8_t cmd, uint32_t addr, uint32_t timeout)
{
    if (addr >= f->size) {
        return HAL_EINVAL;
    }
    hal_status_t st = w25q_wait(f, TIMEOUT_PAGE_MS);
    if (st != HAL_OK) {
        return st;
    }
    (void)write_enable(f);
    select(f);
    send_cmd_addr(f, cmd, addr);
    deselect(f);
    return w25q_wait(f, timeout);
}

hal_status_t w25q_erase_sector(const w25q_t *f, uint32_t addr)
{
    return erase(f, CMD_SECTOR_ERASE, addr & ~(W25Q_SECTOR_SIZE - 1U), TIMEOUT_SECTOR_MS);
}

hal_status_t w25q_erase_block(const w25q_t *f, uint32_t addr)
{
    return erase(f, CMD_BLOCK_ERASE, addr & ~(W25Q_BLOCK_SIZE - 1U), TIMEOUT_BLOCK_MS);
}

hal_status_t w25q_erase_chip(const w25q_t *f)
{
    hal_status_t st = w25q_wait(f, TIMEOUT_PAGE_MS);
    if (st != HAL_OK) {
        return st;
    }
    (void)write_enable(f);
    simple_cmd(f, CMD_CHIP_ERASE);
    return w25q_wait(f, TIMEOUT_CHIP_MS);
}

void w25q_unique_id(const w25q_t *f, uint8_t id[8])
{
    select(f);
    (void)hal_spi_transfer(f->spi, CMD_UNIQUE_ID);
    hal_spi_transfer_buf(f->spi, NULL, NULL, 4U);   /* 4 dummy bytes */
    hal_spi_transfer_buf(f->spi, NULL, id, 8U);
    deselect(f);
}

void w25q_power_down(const w25q_t *f)
{
    simple_cmd(f, CMD_POWER_DOWN);
}

void w25q_wake(const w25q_t *f)
{
    simple_cmd(f, CMD_RELEASE_PD);
    hal_delay_us(5U);                     /* tRES1 = 3 us */
}

const char *w25q_name(const w25q_t *f)
{
    switch (f->jedec[2]) {
    case 0x14U: return "8 Mbit (1 MiB)";
    case 0x15U: return "16 Mbit (2 MiB)";
    case 0x16U: return "32 Mbit (4 MiB)";
    case 0x17U: return "64 Mbit (8 MiB)";
    case 0x18U: return "128 Mbit (16 MiB)";
    default:    return "unknown size";
    }
}

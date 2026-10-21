/**
 * @file    onewire.c
 * @brief   1-Wire and DS18B20 (see onewire.h).
 */
#include "onewire.h"
#include "hal_time.h"
#include "hal_irq.h"
#include "crc.h"

#include <stddef.h>

/* Standard speed timings (Maxim AN126), microseconds */
#define OW_T_RESET_LOW     (480U)
#define OW_T_PRESENCE_WAIT (70U)
#define OW_T_RESET_END     (410U)
#define OW_T_WRITE1_LOW    (6U)
#define OW_T_WRITE1_HIGH   (64U)
#define OW_T_WRITE0_LOW    (60U)
#define OW_T_WRITE0_HIGH   (10U)
#define OW_T_READ_LOW      (6U)
#define OW_T_READ_SAMPLE   (9U)
#define OW_T_READ_END      (55U)

#define OW_CMD_SEARCH_ROM  (0xF0U)
#define OW_CMD_MATCH_ROM   (0x55U)
#define OW_CMD_SKIP_ROM    (0xCCU)
#define DS_CMD_CONVERT     (0x44U)
#define DS_CMD_READ_SP     (0xBEU)
#define DS_CMD_WRITE_SP    (0x4EU)
#define DS_SCRATCHPAD_LEN  (9U)

static void line_low(const onewire_t *ow)     { hal_gpio_clear(ow->pin); }
static void line_release(const onewire_t *ow) { hal_gpio_set(ow->pin); }   /* open drain */

void ow_init(onewire_t *ow, hal_pin_t pin)
{
    ow->pin = pin;
    hal_gpio_set(pin);
    hal_gpio_mode(pin, HAL_GPIO_OUTPUT_OD);
    ow->last_discrepancy = -1;
    ow->last_device      = false;
}

bool ow_reset(onewire_t *ow)
{
    line_low(ow);
    hal_delay_us(OW_T_RESET_LOW);
    const uint32_t irq = hal_irq_save();
    line_release(ow);
    hal_delay_us(OW_T_PRESENCE_WAIT);
    const bool present = !hal_gpio_read(ow->pin);   /* device pulls low */
    hal_irq_restore(irq);
    hal_delay_us(OW_T_RESET_END);
    return present;
}

void ow_write_bit(onewire_t *ow, bool bit)
{
    const uint32_t irq = hal_irq_save();
    line_low(ow);
    hal_delay_us(bit ? OW_T_WRITE1_LOW : OW_T_WRITE0_LOW);
    line_release(ow);
    hal_irq_restore(irq);
    hal_delay_us(bit ? OW_T_WRITE1_HIGH : OW_T_WRITE0_HIGH);
}

bool ow_read_bit(onewire_t *ow)
{
    const uint32_t irq = hal_irq_save();
    line_low(ow);
    hal_delay_us(OW_T_READ_LOW);
    line_release(ow);
    hal_delay_us(OW_T_READ_SAMPLE);
    const bool bit = hal_gpio_read(ow->pin);         /* sample within 15 us */
    hal_irq_restore(irq);
    hal_delay_us(OW_T_READ_END);
    return bit;
}

void ow_write_byte(onewire_t *ow, uint8_t byte)
{
    for (uint32_t i = 0U; i < 8U; i++) {
        ow_write_bit(ow, ((byte >> i) & 1U) != 0U);  /* LSB first */
    }
}

uint8_t ow_read_byte(onewire_t *ow)
{
    uint8_t byte = 0U;
    for (uint32_t i = 0U; i < 8U; i++) {
        if (ow_read_bit(ow)) {
            byte |= (uint8_t)(1U << i);
        }
    }
    return byte;
}

bool ow_select(onewire_t *ow, const uint8_t *rom)
{
    if (!ow_reset(ow)) {
        return false;
    }
    if (rom == NULL) {
        ow_write_byte(ow, OW_CMD_SKIP_ROM);
    } else {
        ow_write_byte(ow, OW_CMD_MATCH_ROM);
        for (uint32_t i = 0U; i < OW_ROM_SIZE; i++) {
            ow_write_byte(ow, rom[i]);
        }
    }
    return true;
}

bool ow_rom_valid(const uint8_t rom[OW_ROM_SIZE])
{
    return crc8_maxim(CRC8_MAXIM_INIT, rom, OW_ROM_SIZE - 1U) == rom[OW_ROM_SIZE - 1U];
}

/* One pass of the search tree (AN187 "OWSearch"). */
static bool search(onewire_t *ow, uint8_t rom[OW_ROM_SIZE])
{
    int32_t last_zero = -1;

    if (ow->last_device || !ow_reset(ow)) {
        ow->last_discrepancy = -1;
        ow->last_device      = false;
        return false;
    }
    ow_write_byte(ow, OW_CMD_SEARCH_ROM);
    for (int32_t bit = 0; bit < 64; bit++) {
        const uint32_t byte_i = (uint32_t)bit / 8U;
        const uint8_t  mask   = (uint8_t)(1U << ((uint32_t)bit % 8U));
        const bool     id     = ow_read_bit(ow);    /* bit of every device ANDed */
        const bool     cmp    = ow_read_bit(ow);    /* complement            */
        bool           dir;

        if (id && cmp) {
            return false;                           /* nobody answered */
        }
        if (id != cmp) {
            dir = id;                               /* all devices agree */
        } else if (bit < ow->last_discrepancy) {
            dir = (ow->rom[byte_i] & mask) != 0U;   /* follow the previous path */
        } else {
            dir = (bit == ow->last_discrepancy);    /* take the 1 branch this time */
        }
        if (!dir && (id == cmp)) {
            last_zero = bit;                        /* remember where we took 0 */
        }
        if (dir) {
            ow->rom[byte_i] |= mask;
        } else {
            ow->rom[byte_i] &= (uint8_t)~mask;
        }
        ow_write_bit(ow, dir);                      /* deselect the others */
    }
    ow->last_discrepancy = last_zero;
    ow->last_device      = (last_zero < 0);
    for (uint32_t i = 0U; i < OW_ROM_SIZE; i++) {
        rom[i] = ow->rom[i];
    }
    return ow_rom_valid(rom);
}

bool ow_search_first(onewire_t *ow, uint8_t rom[OW_ROM_SIZE])
{
    ow->last_discrepancy = -1;
    ow->last_device      = false;
    for (uint32_t i = 0U; i < OW_ROM_SIZE; i++) {
        ow->rom[i] = 0U;
    }
    return search(ow, rom);
}

bool ow_search_next(onewire_t *ow, uint8_t rom[OW_ROM_SIZE])
{
    return search(ow, rom);
}

/* ---------------------------- DS18B20 ----------------------------- */

int32_t ds18b20_raw_to_c100(int16_t raw)
{
    /* raw is 1/16 C: x * 100 / 16 = x * 25 / 4, rounded */
    const int32_t x = (int32_t)raw * 25;
    return (x >= 0) ? ((x + 2) / 4) : ((x - 2) / 4);
}

uint32_t ds18b20_conv_ms(ds18b20_res_t res)
{
    return 94U << (uint32_t)res;       /* 94, 188, 375, 750 */
}

hal_status_t ds18b20_start(onewire_t *ow, const uint8_t *rom)
{
    if (!ow_select(ow, rom)) {
        return HAL_ENACK;
    }
    ow_write_byte(ow, DS_CMD_CONVERT);
    return HAL_OK;
}

hal_status_t ds18b20_read_c100(onewire_t *ow, const uint8_t *rom, int32_t *c100)
{
    uint8_t sp[DS_SCRATCHPAD_LEN];
    if (!ow_select(ow, rom)) {
        return HAL_ENACK;
    }
    ow_write_byte(ow, DS_CMD_READ_SP);
    for (uint32_t i = 0U; i < DS_SCRATCHPAD_LEN; i++) {
        sp[i] = ow_read_byte(ow);
    }
    if (crc8_maxim(CRC8_MAXIM_INIT, sp, DS_SCRATCHPAD_LEN - 1U) != sp[DS_SCRATCHPAD_LEN - 1U]) {
        return HAL_ERROR;              /* bad wiring, missing pull-up... */
    }
    *c100 = ds18b20_raw_to_c100((int16_t)(((uint16_t)sp[1] << 8) | sp[0]));
    return HAL_OK;
}

hal_status_t ds18b20_set_resolution(onewire_t *ow, const uint8_t *rom, ds18b20_res_t res)
{
    if (!ow_select(ow, rom)) {
        return HAL_ENACK;
    }
    ow_write_byte(ow, DS_CMD_WRITE_SP);
    ow_write_byte(ow, 0x4BU);                         /* TH alarm: 75 C */
    ow_write_byte(ow, 0x46U);                         /* TL alarm: 70 C */
    ow_write_byte(ow, (uint8_t)(((uint32_t)res << 5) | 0x1FU));
    return HAL_OK;
}

hal_status_t ds18b20_measure(onewire_t *ow, int32_t *c100)
{
    const hal_status_t st = ds18b20_start(ow, NULL);
    if (st != HAL_OK) {
        return st;
    }
    hal_delay_ms(ds18b20_conv_ms(DS18B20_RES_12BIT));
    return ds18b20_read_c100(ow, NULL, c100);
}

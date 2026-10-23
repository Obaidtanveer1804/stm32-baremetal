/**
 * @file    onewire.h
 * @brief   Dallas/Maxim 1-Wire bus (bit-banged, open drain + 4.7k
 *          pull-up) and the DS18B20 temperature sensor.
 *
 * Timing-critical: each slot is a few microseconds, so interrupts are
 * masked during a slot (not during the whole transaction).
 * ROM search implements the binary-tree algorithm of Maxim AN187,
 * so several sensors on one wire are found and addressed.
 */
#ifndef ONEWIRE_H
#define ONEWIRE_H

#include "hal_types.h"
#include "hal_gpio.h"

#define OW_ROM_SIZE        (8U)
#define DS18B20_FAMILY     (0x28U)

typedef struct {
    hal_pin_t pin;
    /* search state (AN187) */
    uint8_t   rom[OW_ROM_SIZE];
    int32_t   last_discrepancy;
    bool      last_device;
} onewire_t;

void    ow_init(onewire_t *ow, hal_pin_t pin);
/** Reset pulse; returns true if at least one device answered. */
bool    ow_reset(onewire_t *ow);
void    ow_write_bit(onewire_t *ow, bool bit);
bool    ow_read_bit(onewire_t *ow);
void    ow_write_byte(onewire_t *ow, uint8_t byte);
uint8_t ow_read_byte(onewire_t *ow);
/** Address one device (MATCH ROM) or all of them (rom == NULL: SKIP ROM). */
bool    ow_select(onewire_t *ow, const uint8_t *rom);
/** Enumerate: call ow_search_first, then ow_search_next until false. */
bool    ow_search_first(onewire_t *ow, uint8_t rom[OW_ROM_SIZE]);
bool    ow_search_next(onewire_t *ow, uint8_t rom[OW_ROM_SIZE]);
/** CRC8 of the 7 first bytes must equal byte 7. */
bool    ow_rom_valid(const uint8_t rom[OW_ROM_SIZE]);

/* ---------------------------- DS18B20 ----------------------------- */
typedef enum {
    DS18B20_RES_9BIT  = 0,   /*  94 ms, 0.5 C    */
    DS18B20_RES_10BIT = 1,   /* 188 ms, 0.25 C   */
    DS18B20_RES_11BIT = 2,   /* 375 ms, 0.125 C  */
    DS18B20_RES_12BIT = 3    /* 750 ms, 0.0625 C */
} ds18b20_res_t;

/** Start a conversion on one sensor (or all with rom == NULL). Non blocking. */
hal_status_t ds18b20_start(onewire_t *ow, const uint8_t *rom);
/** Conversion time for a resolution, in ms. */
uint32_t     ds18b20_conv_ms(ds18b20_res_t res);
/** Read the result, in 1/100 C, scratchpad CRC checked. */
hal_status_t ds18b20_read_c100(onewire_t *ow, const uint8_t *rom, int32_t *c100);
hal_status_t ds18b20_set_resolution(onewire_t *ow, const uint8_t *rom, ds18b20_res_t res);
/** Blocking convenience: start, wait, read (single sensor bus). */
hal_status_t ds18b20_measure(onewire_t *ow, int32_t *c100);
/** Raw 16-bit register (1/16 C) -> 1/100 C (portable, unit-tested). */
int32_t      ds18b20_raw_to_c100(int16_t raw);

#endif /* ONEWIRE_H */

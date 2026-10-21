/**
 * @file    config.h
 * @brief   Persistent key/value settings in the internal flash, with
 *          wear levelling and power-loss safety.
 *
 * Log-structured store in two flash pages (A/B):
 *  - each "set" appends a record [key][len][value][crc16] in the
 *    active page: no erase for every write (flash survives ~10k erases)
 *  - reading scans for the LAST valid record of a key
 *  - when the page is full, the latest value of every key is copied
 *    to the other page, which then becomes active (garbage collection)
 *  - a record whose CRC fails (power cut while writing) is ignored
 *
 *   config_init();
 *   config_set_u32(CFG_KEY_BAUD, 115200);
 *   uint32_t baud = config_get_u32(CFG_KEY_BAUD, 9600);   // default
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "hal_types.h"

#define CONFIG_VALUE_MAX   (32U)   /* bytes per value */
#define CONFIG_KEYS_MAX    (32U)   /* keys 1..31 (0 and 0xFF reserved) */

hal_status_t config_init(void);
hal_status_t config_set(uint8_t key, const void *value, uint8_t len);
/** Returns the value length, or -1 if the key does not exist. */
int32_t      config_get(uint8_t key, void *value, uint8_t max_len);
hal_status_t config_delete(uint8_t key);
hal_status_t config_set_u32(uint8_t key, uint32_t value);
uint32_t     config_get_u32(uint8_t key, uint32_t default_value);
hal_status_t config_set_str(uint8_t key, const char *s);
bool         config_get_str(uint8_t key, char *buf, uint8_t size);
/** Erase everything (factory reset). */
hal_status_t config_format(void);
/** Bytes used in the active page, for "config info". */
uint32_t     config_used(void);
uint32_t     config_capacity(void);
uint32_t     config_generation(void);
/** Visit every live key (for "config list"). */
void         config_foreach(void (*fn)(uint8_t key, const uint8_t *value, uint8_t len, void *arg), void *arg);

#endif /* CONFIG_H */

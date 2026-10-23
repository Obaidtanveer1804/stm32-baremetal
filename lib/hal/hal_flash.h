/**
 * @file    hal_flash.h
 * @brief   Internal flash erase / program, for non-volatile settings.
 *
 * F1 rules: erase works on whole pages (1 KiB on F100/F103 medium
 * density), an erased byte reads 0xFF, programming is by 16-bit
 * half-words and can only turn 1 bits into 0 bits.
 *
 * The HAL reserves the last HAL_FLASH_USER_PAGES pages of the chip for
 * user data. The linker script must not put code there (programs of a
 * few kB never reach the end of the 128 kB flash anyway).
 * On the PC the flash is a RAM array saved to "flash_sim.bin".
 */
#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include "hal_types.h"

#define HAL_FLASH_PAGE_SIZE    (1024U)
#define HAL_FLASH_USER_PAGES   (4U)

/** Start address of the user area (last pages of the flash). */
uint32_t     hal_flash_user_base(void);
uint32_t     hal_flash_user_size(void);
hal_status_t hal_flash_erase_page(uint32_t addr);
/** @p addr and @p len must be even; data is written as half-words. */
hal_status_t hal_flash_write(uint32_t addr, const void *data, size_t len);
/** Plain memory read (flash is memory-mapped on the MCU). */
void         hal_flash_read(uint32_t addr, void *data, size_t len);

#endif /* HAL_FLASH_H */

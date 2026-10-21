/**
 * @file    hal_sys.h
 * @brief   Chip-level services: clocks, reset, IDs, low power, safe
 *          memory access for debug tools (CLI peek/poke).
 */
#ifndef HAL_SYS_H
#define HAL_SYS_H

#include "hal_types.h"

typedef enum {
    HAL_RESET_UNKNOWN  = 0,
    HAL_RESET_POWER_ON = (1U << 0),
    HAL_RESET_PIN      = (1U << 1),   /**< NRST button              */
    HAL_RESET_SOFTWARE = (1U << 2),
    HAL_RESET_IWDG     = (1U << 3),   /**< independent watchdog     */
    HAL_RESET_WWDG     = (1U << 4),
    HAL_RESET_LOW_POWER = (1U << 5)
} hal_reset_cause_t;

/** Clock tree setup (24 MHz on the F100 VL-Discovery, 72 MHz on F103). */
void        hal_sys_init(void);
uint32_t    hal_sys_clock_hz(void);
const char *hal_sys_name(void);
/** Software reset (NVIC SYSRESETREQ). On the PC: exit the program. */
void        hal_sys_reset(void) __attribute__((noreturn));
/** Bit set of hal_reset_cause_t flags, read once at boot. */
uint32_t    hal_sys_reset_cause(void);
void        hal_sys_unique_id(uint32_t id[3]);
uint32_t    hal_sys_flash_size_kb(void);
uint32_t    hal_sys_ram_size_kb(void);
/**
 * CPU cycle counter (DWT CYCCNT on the Cortex-M3), for profiling:
 *   uint32_t c0 = hal_sys_cycles(); work(); uint32_t n = hal_sys_cycles() - c0;
 * Wraps every 2^32 cycles (~3 min at 24 MHz). The PC port converts real
 * time to cycles of a 24 MHz part, so numbers are comparable but not exact.
 */
void        hal_sys_cycles_init(void);
uint32_t    hal_sys_cycles(void);
/** Sleep until the next interrupt (WFI). */
void        hal_sys_idle(void);
/** Bytes of stack never touched since boot (stack painting). */
uint32_t    hal_sys_stack_free(void);

/** Read/write a 32-bit word only if the address is a valid region
 *  (flash, SRAM, peripherals). Returns false instead of hard-faulting. */
bool        hal_sys_mem_read32(uint32_t addr, uint32_t *value);
bool        hal_sys_mem_write32(uint32_t addr, uint32_t value);
bool        hal_sys_mem_read8(uint32_t addr, uint8_t *value);

#endif /* HAL_SYS_H */

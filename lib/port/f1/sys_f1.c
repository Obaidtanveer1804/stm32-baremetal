/**
 * @file    sys_f1.c
 * @brief   hal_sys + hal_irq for STM32F1 (libopencm3).
 */
#include "hal_sys.h"
#include "hal_irq.h"
#include "f1_common.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/dwt.h>

/* Linker symbols (cortex-m-generic.ld): end of .bss and top of RAM. */
extern uint32_t end;
extern uint32_t _stack;

#define STACK_PAINT_PATTERN   (0xC0FFEE55UL)
#define STACK_PAINT_MARGIN    (64U)          /* words kept below the live SP */

#define FLASH_BASE_ADDR       (0x08000000UL)
#define SRAM_BASE_ADDR        (0x20000000UL)
#define PERIPH_BASE_ADDR      (0x40000000UL)
#define PERIPH_END_ADDR       (0x40024000UL)  /* up to CRC / flash interface */
#define SYSMEM_BASE_ADDR      (0x1FFFF000UL)  /* bootloader + option bytes   */
#define SYSMEM_END_ADDR       (0x1FFFF810UL)
#define PPB_BASE_ADDR         (0xE0000000UL)  /* SysTick, NVIC, SCB, DWT    */
#define PPB_END_ADDR          (0xE0100000UL)

static uint32_t s_reset_cause;

static void stack_paint(void)
{
    uint32_t  sp;
    uint32_t *p = &end;

    __asm__ volatile ("mov %0, sp" : "=r" (sp));
    /* stop a little below the live stack pointer */
    uint32_t *limit = (uint32_t *)(sp - (STACK_PAINT_MARGIN * 4U));

    while (p < limit) {
        *p++ = STACK_PAINT_PATTERN;
    }
}

static void read_reset_cause(void)
{
    const uint32_t csr = RCC_CSR;

    s_reset_cause = HAL_RESET_UNKNOWN;
    if ((csr & RCC_CSR_PORRSTF) != 0U)  { s_reset_cause |= HAL_RESET_POWER_ON; }
    if ((csr & RCC_CSR_PINRSTF) != 0U)  { s_reset_cause |= HAL_RESET_PIN; }
    if ((csr & RCC_CSR_SFTRSTF) != 0U)  { s_reset_cause |= HAL_RESET_SOFTWARE; }
    if ((csr & RCC_CSR_IWDGRSTF) != 0U) { s_reset_cause |= HAL_RESET_IWDG; }
    if ((csr & RCC_CSR_WWDGRSTF) != 0U) { s_reset_cause |= HAL_RESET_WWDG; }
    if ((csr & RCC_CSR_LPWRRSTF) != 0U) { s_reset_cause |= HAL_RESET_LOW_POWER; }
    RCC_CSR |= RCC_CSR_RMVF;     /* clear flags for the next boot */
}

void hal_sys_init(void)
{
    read_reset_cause();
#if defined(STM32F10X_LD_VL) || defined(STM32F10X_MD_VL)
    /* F100 value line: 24 MHz max, 8 MHz HSE crystal on the VL-Discovery */
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_24MHZ]);
#else
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
#endif
    rcc_periph_clock_enable(RCC_AFIO);
    gpio_primary_remap(F1_SWJ_CONFIG, 0U);     /* JTAG pins become GPIO */
    stack_paint();
}

uint32_t hal_sys_clock_hz(void)
{
    return rcc_ahb_frequency;
}

const char *hal_sys_name(void)
{
#if defined(STM32F10X_LD_VL) || defined(STM32F10X_MD_VL)
    return "STM32F100 (Cortex-M3)";
#else
    return "STM32F103 (Cortex-M3)";
#endif
}

void hal_sys_reset(void)
{
    scb_reset_system();
}

uint32_t hal_sys_reset_cause(void)
{
    return s_reset_cause;
}

void hal_sys_unique_id(uint32_t id[3])
{
    desig_get_unique_id(id);
}

uint32_t hal_sys_flash_size_kb(void)
{
    return desig_get_flash_size();
}

uint32_t hal_sys_ram_size_kb(void)
{
    return ((uint32_t)&_stack - SRAM_BASE_ADDR) / 1024U;
}

void hal_sys_cycles_init(void)
{
    (void)dwt_enable_cycle_counter();   /* also enables trace (DEMCR.TRCENA) */
}

uint32_t hal_sys_cycles(void)
{
    return dwt_read_cycle_counter();
}

void hal_sys_idle(void)
{
    __asm__ volatile ("wfi");
}

uint32_t hal_sys_stack_free(void)
{
    const uint32_t *p = &end;
    uint32_t        n = 0U;

    while ((p < &_stack) && (*p == STACK_PAINT_PATTERN)) {
        p++;
        n++;
    }
    return n * 4U;
}

static bool addr_valid(uint32_t addr, uint32_t size)
{
    const uint32_t flash_end = FLASH_BASE_ADDR + (hal_sys_flash_size_kb() * 1024U);
    const uint32_t ram_end   = (uint32_t)&_stack;
    const uint32_t last      = addr + size - 1U;

    if ((addr & (size - 1U)) != 0U) {
        return false;                         /* unaligned access */
    }
    return ((addr >= FLASH_BASE_ADDR)  && (last < flash_end))      ||
           ((addr >= SRAM_BASE_ADDR)   && (last < ram_end))        ||
           ((addr >= PERIPH_BASE_ADDR) && (last < PERIPH_END_ADDR)) ||
           ((addr >= SYSMEM_BASE_ADDR) && (last < SYSMEM_END_ADDR)) ||
           ((addr >= PPB_BASE_ADDR)    && (last < PPB_END_ADDR));
}

bool hal_sys_mem_read32(uint32_t addr, uint32_t *value)
{
    if (!addr_valid(addr, 4U)) {
        return false;
    }
    *value = *(volatile const uint32_t *)addr;
    return true;
}

bool hal_sys_mem_write32(uint32_t addr, uint32_t value)
{
    /* flash cannot be written like RAM: refuse it here */
    if (!addr_valid(addr, 4U) || (addr < SRAM_BASE_ADDR)) {
        return false;
    }
    *(volatile uint32_t *)addr = value;
    return true;
}

bool hal_sys_mem_read8(uint32_t addr, uint8_t *value)
{
    if (!addr_valid(addr, 1U)) {
        return false;
    }
    *value = *(volatile const uint8_t *)addr;
    return true;
}

/* ------------------------------ hal_irq --------------------------- */

uint32_t hal_irq_save(void)
{
    /* returns the previous PRIMASK and masks interrupts */
    return cm_mask_interrupts(1U);
}

void hal_irq_restore(uint32_t state)
{
    (void)cm_mask_interrupts(state);
}

void hal_irq_enable_all(void)
{
    cm_enable_interrupts();
}

void hal_irq_disable_all(void)
{
    cm_disable_interrupts();
}

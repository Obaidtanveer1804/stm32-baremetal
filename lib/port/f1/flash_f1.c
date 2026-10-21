/**
 * @file    flash_f1.c
 * @brief   hal_flash on the F1 flash interface (FPEC).
 *
 * Code executes from flash: while a page is erased / programmed the
 * CPU stalls on any flash fetch, so interrupts are masked during the
 * operation (they would also execute from flash).
 */
#include "hal_flash.h"
#include "hal_sys.h"
#include "hal_irq.h"
#include "strutil.h"

#include <libopencm3/stm32/flash.h>

#define FLASH_START  (0x08000000UL)

uint32_t hal_flash_user_size(void)
{
    return HAL_FLASH_USER_PAGES * HAL_FLASH_PAGE_SIZE;
}

uint32_t hal_flash_user_base(void)
{
    return FLASH_START + (hal_sys_flash_size_kb() * 1024U) - hal_flash_user_size();
}

static bool in_user_area(uint32_t addr, size_t len)
{
    const uint32_t base = hal_flash_user_base();
    return (addr >= base) && ((addr + len) <= (base + hal_flash_user_size())) && (len > 0U);
}

static hal_status_t check_status(void)
{
    const uint32_t sr = flash_get_status_flags();
    if ((sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0U) {
        flash_clear_status_flags();
        return HAL_ERROR;
    }
    return HAL_OK;
}

hal_status_t hal_flash_erase_page(uint32_t addr)
{
    if (!in_user_area(addr, 1U)) {
        return HAL_EINVAL;      /* never erase our own program */
    }
    addr &= ~(HAL_FLASH_PAGE_SIZE - 1U);
    const uint32_t irq = hal_irq_save();
    flash_unlock();
    flash_erase_page(addr);
    flash_lock();
    hal_irq_restore(irq);
    return check_status();
}

hal_status_t hal_flash_write(uint32_t addr, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    if (((addr & 1U) != 0U) || ((len & 1U) != 0U) || !in_user_area(addr, len)) {
        return HAL_EINVAL;
    }
    hal_status_t st = HAL_OK;
    const uint32_t irq = hal_irq_save();
    flash_unlock();
    for (size_t i = 0U; (i < len) && (st == HAL_OK); i += 2U) {
        const uint16_t half = (uint16_t)(p[i] | ((uint16_t)p[i + 1U] << 8));
        flash_program_half_word(addr + i, half);
        st = check_status();
        /* read back: catches writing over non-erased bits */
        if ((st == HAL_OK) && (*(volatile const uint16_t *)(addr + i) != half)) {
            st = HAL_ERROR;
        }
    }
    flash_lock();
    hal_irq_restore(irq);
    return st;
}

void hal_flash_read(uint32_t addr, void *data, size_t len)
{
    const volatile uint8_t *src = (const volatile uint8_t *)addr;
    uint8_t                *dst = (uint8_t *)data;
    for (size_t i = 0U; i < len; i++) {
        dst[i] = src[i];
    }
}

/*
 * STM32F100RB (STM32VL-Discovery): 128 KiB flash, 8 KiB RAM.
 * The last 4 KiB of flash are kept free for hal_flash user data
 * (HAL_FLASH_USER_PAGES = 4 pages of 1 KiB).
 */
MEMORY
{
    rom (rx)  : ORIGIN = 0x08000000, LENGTH = 124K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 8K
}

INCLUDE cortex-m-generic.ld

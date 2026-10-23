/*
 * STM32F103C8 (Blue Pill): 64 KiB flash, 20 KiB RAM.
 * The last 4 KiB of flash are kept free for hal_flash user data.
 */
MEMORY
{
    rom (rx)  : ORIGIN = 0x08000000, LENGTH = 60K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

INCLUDE cortex-m-generic.ld

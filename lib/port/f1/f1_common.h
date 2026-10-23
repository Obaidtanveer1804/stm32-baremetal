/**
 * @file    f1_common.h
 * @brief   Private settings shared by the files of the F1 port.
 */
#ifndef F1_COMMON_H
#define F1_COMMON_H

#include <libopencm3/stm32/gpio.h>

/*
 * Debug port configuration, re-applied by every AFIO remap because the
 * SWJ_CFG bits of AFIO_MAPR are write-only (RM0008 9.4.2).
 * JTAG off, SWD on: frees PA15, PB3 and PB4 for GPIO while keeping the
 * ST-Link (SWDIO PA13, SWCLK PA14) working.
 */
#define F1_SWJ_CONFIG  AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON

#endif /* F1_COMMON_H */

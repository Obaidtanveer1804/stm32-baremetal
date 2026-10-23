/**
 * @file    cli_cmds.h
 * @brief   Ready-made command groups. Register only what an app needs:
 *
 *   cli_register(cmd_sys_group());    // info, uptime, reset, log, top...
 *   cli_register(cmd_gpio_group());   // gpio, led, pulse, blink
 *   ...
 *   cli_register_all();               // everything (big apps / PC)
 */
#ifndef CLI_CMDS_H
#define CLI_CMDS_H

#include "cli.h"

/*
 * Feature switches for small chips (64 kB Blue Pill): build with
 * -DCLI_LITE to drop the biggest optional commands, or set each one.
 */
#ifdef CLI_LITE
#ifndef CLI_WITH_DSP
#define CLI_WITH_DSP      (0)    /* adc fft / adc freq          */
#endif
#ifndef CLI_WITH_DISPLAY
#define CLI_WITH_DISPLAY  (0)    /* oled                        */
#endif
#ifndef CLI_WITH_GPS
#define CLI_WITH_GPS      (0)    /* nmea / geo                  */
#endif
#endif
#ifndef CLI_WITH_DSP
#define CLI_WITH_DSP      (1)
#endif
#ifndef CLI_WITH_DISPLAY
#define CLI_WITH_DISPLAY  (1)
#endif
#ifndef CLI_WITH_GPS
#define CLI_WITH_GPS      (1)
#endif

const cli_group_t *cmd_sys_group(void);      /* info uptime reset echo delay log top */
const cli_group_t *cmd_gpio_group(void);     /* gpio led pulse blink button          */
const cli_group_t *cmd_analog_group(void);   /* adc dac scope                        */
const cli_group_t *cmd_timer_group(void);    /* timer pwm servo tone                 */
const cli_group_t *cmd_mem_group(void);      /* peek poke dump reg bit               */
const cli_group_t *cmd_bus_group(void);      /* i2c spi uart                         */
const cli_group_t *cmd_rtc_group(void);      /* date alarm                           */
const cli_group_t *cmd_util_group(void);     /* calc conv crc morse seg7 rand bits   */
const cli_group_t *cmd_counter_group(void);  /* count (TP2 counters)                 */
const cli_group_t *cmd_dev_group(void);      /* eeprom flash lm75 ds1307 config      */
#ifdef BOARD_PC
const cli_group_t *cmd_sim_group(void);      /* sim (PC only)                        */
#endif

/** Register every group above. */
void cli_register_all(void);

#endif /* CLI_CMDS_H */

/**
 * @file    cli_cmds.c
 * @brief   Register every command group at once.
 */
#include "cli_cmds.h"

void cli_register_all(void)
{
    (void)cli_register(cmd_sys_group());
    (void)cli_register(cmd_gpio_group());
    (void)cli_register(cmd_analog_group());
    (void)cli_register(cmd_timer_group());
    (void)cli_register(cmd_mem_group());
    (void)cli_register(cmd_bus_group());
    (void)cli_register(cmd_rtc_group());
    (void)cli_register(cmd_dev_group());
    (void)cli_register(cmd_util_group());
    (void)cli_register(cmd_counter_group());
#ifdef BOARD_PC
    (void)cli_register(cmd_sim_group());
#endif
}

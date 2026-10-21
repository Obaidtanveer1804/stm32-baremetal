/**
 * @file    thermostat.c
 * @brief   Closed-loop temperature control: LM75 sensor -> fixed-point
 *          PID -> PWM heater, with a live ASCII chart of the response.
 *
 *   LM75 on I2C1 (PB6/PB7), heater MOSFET gate on PB8 (TIM4 CH3 PWM)
 *   keys: '+' / '-' change the setpoint by 1 C, 'q' quits (PC)
 *
 * PC: a first-order thermal model (heater power vs losses to a 20 C
 * room, time constant ~30 s, simulated 10x faster) closes the loop
 * through the simulated LM75, so you can tune the gains on the laptop.
 */
#include "hal.h"
#include "io.h"
#include "pid.h"
#include "filter.h"
#include "sensors_i2c.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define HEATER_PWM        HAL_PWM_T4C3
#define HEATER_PWM_HZ     (1000U)
#define CONTROL_PERIOD_MS (100U)
#define CHART_WIDTH       (50)
#define CHART_MIN_C10     (150)
#define CHART_MAX_C10     (650)

static void chart_line(int32_t temp_c10, int32_t setpoint_c10, uint32_t duty)
{
    const int32_t span = CHART_MAX_C10 - CHART_MIN_C10;
    int32_t pos_t = ((temp_c10 - CHART_MIN_C10) * CHART_WIDTH) / span;
    int32_t pos_s = ((setpoint_c10 - CHART_MIN_C10) * CHART_WIDTH) / span;
    pos_t = (pos_t < 0) ? 0 : ((pos_t >= CHART_WIDTH) ? (CHART_WIDTH - 1) : pos_t);
    pos_s = (pos_s < 0) ? 0 : ((pos_s >= CHART_WIDTH) ? (CHART_WIDTH - 1) : pos_s);

    io_printf("%3ld.%ld C |", (long)(temp_c10 / 10), (long)((temp_c10 < 0 ? -temp_c10 : temp_c10) % 10));
    for (int32_t i = 0; i < CHART_WIDTH; i++) {
        io_putc((i == pos_t) ? '*' : ((i == pos_s) ? '|' : ' '));
    }
    io_printf("| heat %3lu%%" IO_NEWLINE, (unsigned long)(duty / 10U));
}

#ifdef BOARD_PC
/* dT/dt = (P * k_heat - (T - T_room) * k_loss): Euler step per period */
static void plant_step(uint32_t duty_permille)
{
    static int32_t temp_mc = 20000;                   /* milli-degrees */
    const int32_t  room_mc = 20000;
    const int32_t  heat    = (int32_t)duty_permille * 12;            /* up to +12 C/s */
    const int32_t  loss    = (temp_mc - room_mc) / 3;                /* tau ~3 s (x10 speed) */
    temp_mc += ((heat - loss) * (int32_t)CONTROL_PERIOD_MS) / 1000;
    sim_set_temperature_c10(temp_mc / 100);
}
#endif

int main(void)
{
    pid_ctrl_t  pid;
    filt_ema_t  smooth;
    int32_t     setpoint_c10 = 450;       /* 45.0 C */
    uint32_t    step         = 0U;

    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
#ifdef BOARD_PC
    sim_devices_install_default();
    sim_set_temperature_c10(200);
#endif
    (void)hal_i2c_init(HAL_I2C1, 100000U);
    (void)hal_pwm_init(HEATER_PWM, HEATER_PWM_HZ);

    /* gains per 100 ms step, error in 0.1 C, output in per-mille */
    pid_init(&pid, Q16_CONST(8.0), Q16_CONST(0.4), Q16_CONST(10.0), 0, (int32_t)HAL_PWM_DUTY_MAX);
    filt_ema_init(&smooth, 2U);
    io_println("thermostat: '+'/'-' setpoint, 'q' quit");

    uint32_t next = hal_time_ms();
    for (;;) {
        int32_t raw_c10 = 0;
        if (lm75_read_c10(HAL_I2C1, LM75_ADDR_DEFAULT, &raw_c10) != HAL_OK) {
            hal_pwm_set_duty(HEATER_PWM, 0U);      /* sensor lost: heater off! */
            io_println("sensor error, heater off");
            hal_delay_ms(1000U);
            continue;
        }
        const int32_t  temp_c10 = filt_ema_update(&smooth, raw_c10);
        const uint32_t duty     = (uint32_t)pid_update(&pid, setpoint_c10, temp_c10);
        hal_pwm_set_duty(HEATER_PWM, duty);
#ifdef BOARD_PC
        plant_step(duty);
#endif
        if ((step++ % 5U) == 0U) {
            chart_line(temp_c10, setpoint_c10, duty);
        }
        const int c = io_getc();
        if (c == '+') {
            setpoint_c10 += 10;
        } else if (c == '-') {
            setpoint_c10 -= 10;
        } else if ((c == 'q') || (c == IO_CLOSED)) {
            break;
        } else {
            /* no key */
        }
        next += CONTROL_PERIOD_MS;
#ifdef BOARD_PC
        next -= (CONTROL_PERIOD_MS * 9U) / 10U;      /* simulated 10x faster */
#endif
        while ((int32_t)(hal_time_ms() - next) < 0) {
            hal_sys_idle();
        }
    }
    hal_pwm_set_duty(HEATER_PWM, 0U);
    io_println("heater off, bye");
#ifdef BOARD_PC
    sim_shutdown(0);
#endif
    return 0;
}

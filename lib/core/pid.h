/**
 * @file    pid.h
 * @brief   Fixed-point PID controller (Q16.16 gains), no floats.
 *
 * - Derivative on measurement (no "derivative kick" on setpoint steps)
 * - Integrator clamping anti-windup (stops integrating when saturated)
 * - Output limits
 *
 * Gains are per update period: tune them for the rate at which you
 * call pid_update() (e.g. from a 1 kHz timer interrupt).
 *
 * Output limits must stay within +-32767 (the integral is Q16.16).
 *
 * Note: named pid_ctrl_t because pid_t already exists on POSIX (PC).
 */
#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <stdbool.h>
#include "fixmath.h"

typedef struct {
    q16_t   kp;
    q16_t   ki;
    q16_t   kd;
    int32_t out_min;
    int32_t out_max;
    q16_t   integral;        /**< accumulated ki * error, Q16 output units */
    int32_t last_measure;
    bool    first;
} pid_ctrl_t;

void    pid_init(pid_ctrl_t *pid, q16_t kp, q16_t ki, q16_t kd,
                 int32_t out_min, int32_t out_max);
void    pid_set_gains(pid_ctrl_t *pid, q16_t kp, q16_t ki, q16_t kd);
void    pid_reset(pid_ctrl_t *pid);
int32_t pid_update(pid_ctrl_t *pid, int32_t setpoint, int32_t measure);

#endif /* PID_H */

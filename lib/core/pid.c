/**
 * @file    pid.c
 * @brief   Fixed-point PID controller (see pid.h).
 */
#include "pid.h"

void pid_init(pid_ctrl_t *pid, q16_t kp, q16_t ki, q16_t kd,
              int32_t out_min, int32_t out_max)
{
    pid_set_gains(pid, kp, ki, kd);
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid_reset(pid);
}

void pid_set_gains(pid_ctrl_t *pid, q16_t kp, q16_t ki, q16_t kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void pid_reset(pid_ctrl_t *pid)
{
    pid->integral     = 0;
    pid->last_measure = 0;
    pid->first        = true;
}

static int64_t clamp64(int64_t v, int64_t lo, int64_t hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

int32_t pid_update(pid_ctrl_t *pid, int32_t setpoint, int32_t measure)
{
    const int64_t out_min_q16 = (int64_t)pid->out_min * 65536;
    const int64_t out_max_q16 = (int64_t)pid->out_max * 65536;
    const int32_t error       = setpoint - measure;
    int64_t       d_measure   = 0;

    if (pid->first) {
        pid->last_measure = measure;
        pid->first        = false;
    }
    d_measure         = (int64_t)measure - pid->last_measure;
    pid->last_measure = measure;

    /* all terms in Q16: gain (Q16) * integer error = Q16 */
    const int64_t p_term = (int64_t)pid->kp * error;
    const int64_t d_term = -(int64_t)pid->kd * d_measure;
    const int64_t i_next = clamp64((int64_t)pid->integral + ((int64_t)pid->ki * error),
                                   out_min_q16, out_max_q16);

    int64_t out = p_term + i_next + d_term;

    /* anti-windup: only accept the new integral if it does not push
     * the output further into saturation */
    if ((out > out_max_q16) && (error > 0)) {
        out = out_max_q16;
    } else if ((out < out_min_q16) && (error < 0)) {
        out = out_min_q16;
    } else {
        pid->integral = (q16_t)i_next;
    }
    out = clamp64(out, out_min_q16, out_max_q16);
    return (int32_t)((out + Q16_HALF) >> 16);
}

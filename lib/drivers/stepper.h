/**
 * @file    stepper.h
 * @brief   Unipolar stepper (28BYJ-48 + ULN2003) on 4 GPIOs, and
 *          step/dir drivers (A4988, DRV8825) on 2 GPIOs, with a
 *          trapezoidal speed profile driven by stepper_tick().
 *
 * stepper_tick() is meant for a timer interrupt at STEPPER_TICK_HZ;
 * it decides on each tick if a step is due, so several motors can
 * share one timer.
 */
#ifndef STEPPER_H
#define STEPPER_H

#include "hal_gpio.h"

#define STEPPER_TICK_HZ   (10000U)

typedef enum {
    STEPPER_WAVE = 0,     /**< 1 coil at a time, 4 steps/cycle, low torque */
    STEPPER_FULL,         /**< 2 coils, 4 steps/cycle, max torque           */
    STEPPER_HALF,         /**< 8 steps/cycle, smoother, double resolution   */
    STEPPER_STEP_DIR      /**< external driver: pins[0]=STEP pins[1]=DIR    */
} stepper_mode_t;

typedef struct {
    hal_pin_t        pins[4];
    stepper_mode_t   mode;
    uint8_t          phase;
    volatile int32_t position;     /**< steps                       */
    volatile int32_t target;
    uint32_t         max_sps;      /**< steps per second             */
    uint32_t         accel;        /**< steps per second^2           */
    uint32_t         speed_sps;    /**< current speed                */
    uint32_t         accum;        /**< phase accumulator (DDS style) */
    uint32_t         ticks;        /**< for the acceleration ramp    */
} stepper_t;

void stepper_init(stepper_t *m, const hal_pin_t pins[4], stepper_mode_t mode);
void stepper_set_speed(stepper_t *m, uint32_t max_sps, uint32_t accel);
void stepper_move_to(stepper_t *m, int32_t target);
void stepper_move(stepper_t *m, int32_t delta);
void stepper_stop(stepper_t *m);            /**< immediate */
bool stepper_is_running(const stepper_t *m);
/** De-energise the coils (saves power, loses holding torque). */
void stepper_release(stepper_t *m);
/** One step now in a direction (+1 / -1), ignoring the profile. */
void stepper_step(stepper_t *m, int32_t dir);
/** Call at STEPPER_TICK_HZ. */
void stepper_tick(stepper_t *m);

#endif /* STEPPER_H */

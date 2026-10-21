/**
 * @file    stepper.c
 * @brief   Stepper motor control (see stepper.h).
 *
 * Speed profile without division in the ISR: a phase accumulator adds
 * the speed (steps/s) every tick and emits a step when it passes
 * STEPPER_TICK_HZ (like a DDS). The speed ramps by "accel" per second,
 * and starts braking when the remaining distance equals the braking
 * distance v^2 / (2a).
 */
#include "stepper.h"
#include "fixmath.h"

#define MIN_SPEED_SPS  (50U)

/* coil patterns, bit i = pins[i] (IN1..IN4) */
static const uint8_t seq_wave[4] = { 0x1U, 0x2U, 0x4U, 0x8U };
static const uint8_t seq_full[4] = { 0x3U, 0x6U, 0xCU, 0x9U };
static const uint8_t seq_half[8] = { 0x1U, 0x3U, 0x2U, 0x6U, 0x4U, 0xCU, 0x8U, 0x9U };

static void apply_coils(const stepper_t *m, uint8_t pattern)
{
    for (uint32_t i = 0U; i < 4U; i++) {
        hal_gpio_write(m->pins[i], ((pattern >> i) & 1U) != 0U);
    }
}

void stepper_init(stepper_t *m, const hal_pin_t pins[4], stepper_mode_t mode)
{
    const uint32_t n = (mode == STEPPER_STEP_DIR) ? 2U : 4U;
    for (uint32_t i = 0U; i < 4U; i++) {
        m->pins[i] = (i < n) ? pins[i] : HAL_PIN_NONE;
        if (i < n) {
            hal_gpio_mode(pins[i], HAL_GPIO_OUTPUT);
            hal_gpio_clear(pins[i]);
        }
    }
    m->mode      = mode;
    m->phase     = 0U;
    m->position  = 0;
    m->target    = 0;
    m->speed_sps = 0U;
    m->accum     = 0U;
    m->ticks     = 0U;
    stepper_set_speed(m, 500U, 1000U);
}

void stepper_set_speed(stepper_t *m, uint32_t max_sps, uint32_t accel)
{
    m->max_sps = (max_sps > (STEPPER_TICK_HZ / 2U)) ? (STEPPER_TICK_HZ / 2U) : max_sps;
    m->accel   = (accel == 0U) ? 1U : accel;
}

void stepper_step(stepper_t *m, int32_t dir)
{
    if (m->mode == STEPPER_STEP_DIR) {
        hal_gpio_write(m->pins[1], dir > 0);
        hal_gpio_set(m->pins[0]);             /* A4988: >= 1 us pulse */
        hal_gpio_clear(m->pins[0]);
    } else {
        const uint8_t *seq = (m->mode == STEPPER_HALF) ? seq_half
                           : ((m->mode == STEPPER_WAVE) ? seq_wave : seq_full);
        const uint8_t  len = (m->mode == STEPPER_HALF) ? 8U : 4U;
        m->phase = (uint8_t)((m->phase + ((dir > 0) ? 1U : (len - 1U))) % len);
        apply_coils(m, seq[m->phase]);
    }
    m->position += (dir > 0) ? 1 : -1;
}

void stepper_move_to(stepper_t *m, int32_t target)
{
    m->target = target;
}

void stepper_move(stepper_t *m, int32_t delta)
{
    m->target = m->position + delta;
}

void stepper_stop(stepper_t *m)
{
    m->target    = m->position;
    m->speed_sps = 0U;
}

bool stepper_is_running(const stepper_t *m)
{
    return m->position != m->target;
}

void stepper_release(stepper_t *m)
{
    if (m->mode != STEPPER_STEP_DIR) {
        apply_coils(m, 0U);
    }
}

void stepper_tick(stepper_t *m)
{
    const int32_t remaining = m->target - m->position;
    if (remaining == 0) {
        m->speed_sps = 0U;
        m->accum     = 0U;
        return;
    }
    const uint32_t dist = (uint32_t)fx_abs(remaining);

    /* update the speed 100 times per second */
    if (++m->ticks >= (STEPPER_TICK_HZ / 100U)) {
        m->ticks = 0U;
        const uint32_t dv    = (m->accel + 99U) / 100U;
        const uint64_t brake = ((uint64_t)m->speed_sps * m->speed_sps) / (2U * (uint64_t)m->accel);
        if ((uint64_t)dist <= brake) {
            m->speed_sps = (m->speed_sps > (MIN_SPEED_SPS + dv)) ? (m->speed_sps - dv) : MIN_SPEED_SPS;
        } else if (m->speed_sps < m->max_sps) {
            m->speed_sps = ((m->speed_sps + dv) > m->max_sps) ? m->max_sps : (m->speed_sps + dv);
        } else {
            /* cruising */
        }
        if (m->speed_sps < MIN_SPEED_SPS) {
            m->speed_sps = MIN_SPEED_SPS;
        }
    }
    m->accum += m->speed_sps;
    if (m->accum >= STEPPER_TICK_HZ) {
        m->accum -= STEPPER_TICK_HZ;
        stepper_step(m, (remaining > 0) ? 1 : -1);
    }
}

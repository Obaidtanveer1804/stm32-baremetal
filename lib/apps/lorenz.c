/**
 * @file    lorenz.c
 * @brief   Lorenz attractor in Q16.16 fixed point (the lorenz/ TP,
 *          rebuilt on fixmath.h) drawn on the OLED, plus a speed
 *          comparison of fixed point vs soft-float on the same chip.
 *
 *   dx/dt = sigma (y - x)
 *   dy/dt = x (rho - z) - y
 *   dz/dt = x y - beta z          sigma = 10, rho = 28, beta = 8/3
 *
 * Explicit Euler, dt = 1/256 (a power of two: the multiply by dt is a
 * shift). Q16.16 covers +-32768, the attractor stays within +-50.
 */
#include "hal.h"
#include "io.h"
#include "fixmath.h"
#include "gfx.h"
#include "ssd1306.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define LORENZ_DT_SHIFT   (8)              /* dt = 2^-8          */
#define LORENZ_STEPS      (6000U)
#define BENCH_STEPS       (1000U)

typedef struct {
    q16_t x;
    q16_t y;
    q16_t z;
} lorenz_q16_t;

static const q16_t SIGMA = Q16_CONST(10.0);
static const q16_t RHO   = Q16_CONST(28.0);
static const q16_t BETA  = Q16_CONST(2.6666667);

static void lorenz_step_q16(lorenz_q16_t *s)
{
    const q16_t dx = q16_mul(SIGMA, s->y - s->x);
    const q16_t dy = q16_mul(s->x, RHO - s->z) - s->y;
    const q16_t dz = q16_mul(s->x, s->y) - q16_mul(BETA, s->z);
    /* x += dx * dt  ->  arithmetic shift, no multiply */
    s->x += dx >> LORENZ_DT_SHIFT;
    s->y += dy >> LORENZ_DT_SHIFT;
    s->z += dz >> LORENZ_DT_SHIFT;
}

/* the same thing in float, only for the speed comparison */
typedef struct {
    float x;
    float y;
    float z;
} lorenz_f_t;

static void lorenz_step_float(lorenz_f_t *s)
{
    const float dt = 1.0f / 256.0f;
    const float dx = 10.0f * (s->y - s->x);
    const float dy = (s->x * (28.0f - s->z)) - s->y;
    const float dz = (s->x * s->y) - ((8.0f / 3.0f) * s->z);
    s->x += dx * dt;
    s->y += dy * dt;
    s->z += dz * dt;
}

int main(void)
{
    static uint8_t fb[(128 * 64) / 8];
    ssd1306_t      oled;
    gfx_t          view;
    gfx_t         *g;
    lorenz_q16_t   q = { Q16_CONST(1.0), Q16_CONST(1.0), Q16_CONST(1.0) };
    lorenz_f_t     f = { 1.0f, 1.0f, 1.0f };

    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    hal_sys_cycles_init();
#ifdef BOARD_PC
    sim_devices_install_default();
#endif
    (void)hal_i2c_init(HAL_I2C1, 400000U);
    const bool have_oled = (ssd1306_init(&oled, HAL_I2C1, SSD1306_ADDR, 64) == HAL_OK);
    if (have_oled) {
        g = &oled.gfx;
    } else {
        gfx_init(&view, fb, 128, 64);         /* draw in RAM, print to console */
        g = &view;
    }

    /* speed: same number of steps in both arithmetics */
    uint32_t c0 = hal_sys_cycles();
    for (uint32_t i = 0U; i < BENCH_STEPS; i++) {
        lorenz_step_q16(&q);
    }
    const uint32_t cyc_fixed = hal_sys_cycles() - c0;
    c0 = hal_sys_cycles();
    for (uint32_t i = 0U; i < BENCH_STEPS; i++) {
        lorenz_step_float(&f);
    }
    const uint32_t cyc_float = hal_sys_cycles() - c0;
    io_printf("%lu Euler steps: Q16.16 %lu cycles, soft float %lu cycles" IO_NEWLINE,
              (unsigned long)BENCH_STEPS, (unsigned long)cyc_fixed, (unsigned long)cyc_float);
    /* chaos: both start equal, rounding differences grow exponentially */
    io_puts("x after the run: fixed ");
    io_print_fixed((int32_t)(((int64_t)q.x * 100) / 65536), 100U, 2U);
    io_puts("  float ");
    io_print_fixed((int32_t)(f.x * 100.0f), 100U, 2U);
    io_puts("  (same so far; run longer and rounding makes them diverge)" IO_NEWLINE);

    /* draw the x-z butterfly: x in [-25, 25] -> 0..127, z in [0, 50] -> 63..0 */
    gfx_clear(g);
    for (uint32_t i = 0U; i < LORENZ_STEPS; i++) {
        lorenz_step_q16(&q);
        const int32_t px = 64 + ((q16_to_int(q.x * 5) / 2));
        const int32_t py = 63 - ((q16_to_int(q.z * 5) / 4));
        gfx_pixel(g, (int16_t)px, (int16_t)py, GFX_SET);
    }
    gfx_set_cursor(g, 0, 0);
    gfx_puts(g, "Lorenz Q16");
    if (have_oled) {
        (void)ssd1306_flush(&oled);
#ifdef BOARD_PC
        sim_oled_show();
#endif
    } else {
        gfx_print_ascii(g);
    }
#ifdef BOARD_PC
    sim_shutdown(0);
#endif
    for (;;) {
        hal_sys_idle();
    }
    return 0;
}

/**
 * @file    gps_nav.c
 * @brief   Offline turn-by-turn navigation through a list of stops,
 *          on a GPS module + SSD1306 OLED (bike computer style).
 *
 * Hardware:
 *   GPS (NEO-6M/M8N) TX -> PA3 (USART2 RX), 9600 baud, 3.3 V
 *   SSD1306 OLED     -> PB6 SCL / PB7 SDA (I2C1), address 0x3C
 *   buzzer (passive) -> PB8 (TIM4 CH3) : beeps at each stop
 *   USER button      -> skip to the next stop
 *
 * Everything below main() is plain logic on nmea.c + geo.c + gfx.c,
 * so it runs unchanged on the PC: there a simulated rider moves along
 * the route and sends real NMEA sentences into USART2, and the OLED
 * is drawn in the terminal ("make run APP=gps_nav").
 */
#include "hal.h"
#include "io.h"
#include "log.h"
#include "nmea.h"
#include "geo.h"
#include "gfx.h"
#include "ssd1306.h"
#include "button.h"
#include "melody.h"
#include "fixmath.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define GPS_UART            HAL_UART2
#define GPS_BAUD            (9600U)
#define ARRIVAL_RADIUS_M    (25U)
#define OFF_ROUTE_M         (60)
#define SCREEN_PERIOD_MS    (1000U)
#define BUZZER_CH           HAL_PWM_T4C3

typedef struct {
    const char *name;
    geo_point_t pos;
} stop_t;

/* Example round in Besancon (micro-degrees). Replace with your route. */
static const stop_t route[] = {
    { "Depot",         { 47237800, 6024100 } },
    { "Rue Battant",   { 47240600, 6019000 } },
    { "Pont Battant",  { 47239500, 6022300 } },
    { "Granvelle",     { 47236300, 6025300 } },
    { "Gare Viotte",   { 47246600, 6021900 } },
};
#define ROUTE_LEN  (sizeof(route) / sizeof(route[0]))

typedef struct {
    uint32_t stop;          /* index of the stop we are heading to */
    uint32_t delivered;
    uint32_t odometer_m;
    geo_point_t last;
    bool     have_last;
} nav_state_t;

static nav_state_t s_nav = { 1U, 0U, 0U, { 0, 0 }, false };
static nmea_t      s_gps;
static ssd1306_t   s_oled;
static bool        s_oled_ok;
static melody_t    s_beep;
static button_t    s_button;

/* ----------------------------- logic ------------------------------ */

static void next_stop(void)
{
    s_nav.delivered++;
    s_nav.stop = (s_nav.stop + 1U) % ROUTE_LEN;
    melody_play(&s_beep, "c5:8 e5:8 g5:4", 160U);
    LOG_I("nav", "next stop %lu: %s", (unsigned long)s_nav.stop, route[s_nav.stop].name);
}

static void draw_screen(const nmea_fix_t *fix, uint32_t dist_m, int32_t turn_cdeg, int32_t xtrack_m)
{
    gfx_t *g = &s_oled.gfx;
    gfx_clear(g);
    /* left: arrow relative to the riding direction (up = straight on) */
    const uint16_t arrow = (uint16_t)(((int32_t)turn_cdeg * 65536) / 36000);
    gfx_circle(g, 24, 32, 22, GFX_SET);
    gfx_arrow(g, 24, 32, 18, arrow);
    /* right: stop name, distance, speed */
    gfx_set_cursor(g, 52, 0);
    gfx_printf(g, "%lu/%lu", (unsigned long)s_nav.stop, (unsigned long)(ROUTE_LEN - 1U));
    gfx_set_cursor(g, 52, 10);
    gfx_puts(g, route[s_nav.stop].name);
    gfx_set_text_scale(g, 2U);
    gfx_set_cursor(g, 52, 24);
    if (dist_m >= 1000U) {
        gfx_printf(g, "%lu.%luk", (unsigned long)(dist_m / 1000U), (unsigned long)((dist_m % 1000U) / 100U));
    } else {
        gfx_printf(g, "%lum", (unsigned long)dist_m);
    }
    gfx_set_text_scale(g, 1U);
    gfx_set_cursor(g, 52, 44);
    gfx_printf(g, "%lu km/h", (unsigned long)((fix->speed_mmps * 36U) / 10000U));
    gfx_set_cursor(g, 52, 54);
    if ((xtrack_m > OFF_ROUTE_M) || (xtrack_m < -OFF_ROUTE_M)) {
        gfx_puts(g, "OFF ROUTE");
    } else {
        gfx_printf(g, "sat %u", (unsigned)fix->satellites);
    }
    if (s_oled_ok) {
        (void)ssd1306_flush(&s_oled);
    }
}

static void update_navigation(void)
{
    const nmea_fix_t *fix = &s_gps.fix;
    if (!fix->valid) {
        return;
    }
    const geo_point_t here = { fix->lat_udeg, fix->lon_udeg };
    if (s_nav.have_last) {
        s_nav.odometer_m += geo_distance_m(&s_nav.last, &here);
    }
    s_nav.last      = here;
    s_nav.have_last = true;

    const stop_t  *target = &route[s_nav.stop];
    const stop_t  *prev   = &route[(s_nav.stop + ROUTE_LEN - 1U) % ROUTE_LEN];
    const uint32_t dist   = geo_distance_m(&here, &target->pos);
    const uint32_t brg    = geo_bearing_cdeg(&here, &target->pos);
    const int32_t  turn   = geo_turn_cdeg(fix->course_cdeg, brg);
    const int32_t  xtrack = geo_cross_track_m(&prev->pos, &target->pos, &here);

    io_printf("%02u:%02u:%02u  -> %-12s %5lu m  %-3s  turn %+4ld deg  %2lu km/h  odo %lu m" IO_NEWLINE,
              (unsigned)fix->hour, (unsigned)fix->minute, (unsigned)fix->second, target->name,
              (unsigned long)dist, geo_compass(brg), (long)(turn / 100),
              (unsigned long)((fix->speed_mmps * 36U) / 10000U), (unsigned long)s_nav.odometer_m);
    draw_screen(fix, dist, turn, xtrack);
    if (dist <= ARRIVAL_RADIUS_M) {
        LOG_I("nav", "arrived at %s", target->name);
        next_stop();
    }
}

/* --------------------------- PC test bench ------------------------ */
#ifdef BOARD_PC
/* A rider at ~18 km/h following the route, sending one RMC per second
 * on the GPS UART, exactly what the real module would do. */
static void bench_gps_tick(void)
{
    static geo_point_t pos      = { 47237800, 6024100 };
    static uint32_t    leg      = 1U;
    static uint32_t    seconds  = 8U * 3600U;
    const uint32_t     step_m   = 5U;          /* 5 m/s = 18 km/h */
    nmea_fix_t         f;
    char               line[NMEA_MAX_SENTENCE];

    const geo_point_t *goal = &route[leg].pos;
    const uint32_t     d    = geo_distance_m(&pos, goal);
    const uint32_t     brg  = geo_bearing_cdeg(&pos, goal);
    if (d <= step_m) {
        pos = *goal;
        leg = (leg + 1U) % ROUTE_LEN;
    } else {
        /* move step_m along the bearing (plus a little GPS jitter) */
        const int32_t e = (int32_t)(((int64_t)step_m * fx_sin_q15((uint16_t)((brg * 65536U) / 36000U))) / 32767);
        const int32_t n = (int32_t)(((int64_t)step_m * fx_cos_q15((uint16_t)((brg * 65536U) / 36000U))) / 32767);
        geo_move(&pos, e, n, &pos);
    }
    f.valid       = true;
    f.lat_udeg    = pos.lat_udeg;
    f.lon_udeg    = pos.lon_udeg;
    f.speed_mmps  = step_m * 1000U;
    f.course_cdeg = brg;
    f.hour        = (uint8_t)(seconds / 3600U);
    f.minute      = (uint8_t)((seconds / 60U) % 60U);
    f.second      = (uint8_t)(seconds % 60U);
    f.millis      = 0U;
    f.day         = 16U;
    f.month       = 10U;
    f.year        = 2026U;
    seconds++;
    const uint32_t len = nmea_format_rmc(line, sizeof(line), &f);
    sim_uart_inject(GPS_UART, (const uint8_t *)line, len);
}
#endif

/* ------------------------------ main ------------------------------ */

int main(void)
{
    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    log_init(hal_time_ms);
#ifdef BOARD_PC
    sim_devices_install_default();
#endif
    (void)hal_uart_init(GPS_UART, GPS_BAUD);
    (void)hal_i2c_init(HAL_I2C1, 400000U);
    s_oled_ok = (ssd1306_init(&s_oled, HAL_I2C1, SSD1306_ADDR, 64) == HAL_OK);
    if (!s_oled_ok) {
        LOG_W("nav", "no OLED, console output only");
    }
    melody_init(&s_beep, BUZZER_CH);
    button_init(&s_button, BOARD_BUTTON, BOARD_BUTTON_ACTIVE_HIGH != 0, NULL);
    nmea_init(&s_gps);
    LOG_I("nav", "%lu stops, heading to %s", (unsigned long)(ROUTE_LEN - 1U), route[s_nav.stop].name);

    uint32_t last_screen = hal_time_ms();
    for (;;) {
        int c;
        while ((c = hal_uart_getc(GPS_UART)) >= 0) {
            if (nmea_feed(&s_gps, (char)c) == NMEA_SENTENCE_RMC) {
                update_navigation();
            }
        }
        if (button_update(&s_button, hal_time_ms()) == BTN_EVENT_CLICK) {
            next_stop();
        }
        melody_update(&s_beep, hal_time_ms());
#ifdef BOARD_PC
        /* simulated time runs 10x faster so the demo is short */
        if ((hal_time_ms() - last_screen) >= (SCREEN_PERIOD_MS / 10U)) {
            last_screen = hal_time_ms();
            bench_gps_tick();
            if (s_nav.delivered >= (ROUTE_LEN - 1U)) {
                sim_oled_show();
                io_printf("round finished: %lu m" IO_NEWLINE, (unsigned long)s_nav.odometer_m);
                sim_shutdown(0);
            }
        }
#else
        (void)last_screen;
#endif
        hal_sys_idle();
    }
    return 0;
}

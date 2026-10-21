/**
 * @file    geo.c
 * @brief   Integer geodesy (see geo.h).
 */
#include "geo.h"
#include "fixmath.h"

/* Mean Earth radius 6371008.8 m: metres per micro-degree of latitude
 * = 2 * pi * R / 360e6 = 0.111195 m. Stored as Q32 fraction. */
#define GEO_M_PER_UDEG_Q32   (477579233LL)  /* 0.1111949 * 2^32 */
#define UDEG_PER_DEG         (1000000L)
#define BRAD_PER_TURN        (65536L)

/* x * scale / 2^32, rounded to nearest (symmetric for negatives) */
static int32_t scale_q32(int64_t x)
{
    const int64_t p = x * (int64_t)GEO_M_PER_UDEG_Q32;
    return (int32_t)((p >= 0) ? ((p + 2147483648LL) / 4294967296LL) : -((-p + 2147483648LL) / 4294967296LL));
}

/* micro-degrees -> brad (65536 per turn) for the fixmath trig */
static uint16_t udeg_to_brad(int32_t udeg)
{
    const int64_t b = ((int64_t)udeg * BRAD_PER_TURN) / (360L * UDEG_PER_DEG);
    return (uint16_t)(uint32_t)(int32_t)b;
}

/* Local projection: dx (east) and dy (north) in metres. */
void geo_offset_m(const geo_point_t *a, const geo_point_t *b, int32_t *east_m, int32_t *north_m)
{
    int64_t dlon = (int64_t)b->lon_udeg - a->lon_udeg;
    /* wrap across the 180 degree meridian */
    if (dlon > (180L * UDEG_PER_DEG)) {
        dlon -= 360L * UDEG_PER_DEG;
    } else if (dlon < (-180L * UDEG_PER_DEG)) {
        dlon += 360L * UDEG_PER_DEG;
    } else {
        /* no wrap */
    }
    const int64_t dlat    = (int64_t)b->lat_udeg - a->lat_udeg;
    const int32_t mid_lat = (int32_t)(((int64_t)a->lat_udeg + b->lat_udeg) / 2);
    const int64_t cos_q16 = q16_cos(udeg_to_brad(mid_lat));         /* 0..65536 */

    const int64_t x_udeg = (dlon * cos_q16) / 65536;
    *east_m  = scale_q32(x_udeg);
    *north_m = scale_q32(dlat);
}

uint32_t geo_distance_m(const geo_point_t *a, const geo_point_t *b)
{
    int32_t e;
    int32_t n;
    geo_offset_m(a, b, &e, &n);
    return fx_hypot(e, n);
}

uint32_t geo_bearing_cdeg(const geo_point_t *a, const geo_point_t *b)
{
    int32_t e;
    int32_t n;
    geo_offset_m(a, b, &e, &n);
    if ((e == 0) && (n == 0)) {
        return 0U;
    }
    /* math angle is from east counter-clockwise; compass is from north
     * clockwise: bearing = atan2(east, north) */
    const uint32_t brad = q16_atan2(e, n);
    return (uint32_t)(((uint64_t)brad * 36000U + 32768U) / 65536U) % 36000U;
}

void geo_move(const geo_point_t *a, int32_t east_m, int32_t north_m, geo_point_t *out)
{
    const int64_t dlat = ((int64_t)north_m * 4294967296LL) / (int64_t)GEO_M_PER_UDEG_Q32;
    const int64_t lat  = a->lat_udeg + dlat;
    int64_t       cos_q16 = q16_cos(udeg_to_brad((int32_t)((a->lat_udeg + lat) / 2)));
    if (cos_q16 < 1) {
        cos_q16 = 1;                       /* at the poles */
    }
    const int64_t dlon_eq = ((int64_t)east_m * 4294967296LL) / (int64_t)GEO_M_PER_UDEG_Q32;
    out->lat_udeg = (int32_t)lat;
    out->lon_udeg = (int32_t)(a->lon_udeg + ((dlon_eq * 65536) / cos_q16));
}

const char *geo_compass(uint32_t bearing_cdeg)
{
    static const char *const names[16] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    /* 22.5 degree sectors centred on each direction */
    return names[((bearing_cdeg % 36000U) + 1125U) / 2250U % 16U];
}

int32_t geo_turn_cdeg(uint32_t course_cdeg, uint32_t bearing_cdeg)
{
    int32_t d = (int32_t)(bearing_cdeg % 36000U) - (int32_t)(course_cdeg % 36000U);
    if (d > 18000) {
        d -= 36000;
    } else if (d <= -18000) {
        d += 36000;
    } else {
        /* already the short way */
    }
    return d;
}

int32_t geo_cross_track_m(const geo_point_t *a, const geo_point_t *b, const geo_point_t *p)
{
    int32_t bx;
    int32_t by;
    int32_t px;
    int32_t py;
    geo_offset_m(a, b, &bx, &by);
    geo_offset_m(a, p, &px, &py);
    const uint32_t len = fx_hypot(bx, by);
    if (len == 0U) {
        return (int32_t)fx_hypot(px, py);
    }
    /* 2D cross product / |ab| ; sign: right of a->b is positive */
    const int64_t cross = ((int64_t)px * by) - ((int64_t)py * bx);
    return (int32_t)(cross / (int64_t)len);
}

int32_t geo_along_track_permille(const geo_point_t *a, const geo_point_t *b, const geo_point_t *p)
{
    int32_t bx;
    int32_t by;
    int32_t px;
    int32_t py;
    geo_offset_m(a, b, &bx, &by);
    geo_offset_m(a, p, &px, &py);
    const int64_t len2 = ((int64_t)bx * bx) + ((int64_t)by * by);
    if (len2 == 0) {
        return 0;
    }
    const int64_t dot = ((int64_t)px * bx) + ((int64_t)py * by);
    return (int32_t)((dot * 1000) / len2);
}

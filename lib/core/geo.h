/**
 * @file    geo.h
 * @brief   Integer geodesy for small MCUs: distance, bearing, offsets
 *          between GPS points in micro-degrees, no floating point.
 *
 * Uses the equirectangular approximation, x = dlon * cos(lat),
 * y = dlat, d = R * sqrt(x^2 + y^2). Error < 0.1 % below ~20 km,
 * which is what a city delivery route needs. cos() and atan2() come
 * from fixmath (CORDIC), sqrt from the integer square root.
 */
#ifndef GEO_H
#define GEO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t lat_udeg;
    int32_t lon_udeg;
} geo_point_t;

/** Distance in metres between two points (< ~100 km for accuracy). */
uint32_t geo_distance_m(const geo_point_t *a, const geo_point_t *b);
/** Initial bearing from a to b, in 0.01 degree (0 = north, 9000 = east). */
uint32_t geo_bearing_cdeg(const geo_point_t *a, const geo_point_t *b);
/** Local east/north offset of b from a, in metres. */
void     geo_offset_m(const geo_point_t *a, const geo_point_t *b, int32_t *east_m, int32_t *north_m);
/** Point moved by (east, north) metres from a. */
void     geo_move(const geo_point_t *a, int32_t east_m, int32_t north_m, geo_point_t *out);
/** 16-point compass name for a bearing ("N", "NNE", ... "NW"). */
const char *geo_compass(uint32_t bearing_cdeg);
/** Turn instruction from current course to target bearing:
 *  -18000..+18000 cdeg, negative = turn left. */
int32_t  geo_turn_cdeg(uint32_t course_cdeg, uint32_t bearing_cdeg);
/** Cross-track distance of p from the segment a->b (metres, signed:
 *  positive = p is right of the route). For "you left the route". */
int32_t  geo_cross_track_m(const geo_point_t *a, const geo_point_t *b, const geo_point_t *p);
/** Distance along the route segment: 0 at a, 1000 at b (per-mille). */
int32_t  geo_along_track_permille(const geo_point_t *a, const geo_point_t *b, const geo_point_t *p);

#endif /* GEO_H */

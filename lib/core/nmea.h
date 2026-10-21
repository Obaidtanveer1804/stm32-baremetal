/**
 * @file    nmea.h
 * @brief   NMEA 0183 GPS parser (u-blox NEO-6M, NEO-M8N, ATGM336H...),
 *          character by character, no floats, no malloc.
 *
 * Feed every byte from the GPS UART (9600 baud by default):
 *
 *   nmea_t gps;  nmea_init(&gps);
 *   int c;
 *   while ((c = hal_uart_getc(HAL_UART2)) >= 0) {
 *       if (nmea_feed(&gps, (char)c) == NMEA_SENTENCE_RMC && gps.fix.valid) { ... }
 *   }
 *
 * Coordinates are stored in micro-degrees (1e-6 deg ~ 0.11 m), which
 * fits an int32 for the whole Earth and is precise enough for a bike.
 * Sentences: RMC (position, speed, course, date), GGA (fix quality,
 * satellites, altitude, HDOP), VTG (speed), GSA (DOP), GLL.
 * Talker IDs GP / GN / GL / GA / BD are all accepted.
 */
#ifndef NMEA_H
#define NMEA_H

#include <stdint.h>
#include <stdbool.h>

#define NMEA_MAX_SENTENCE  (96U)   /* standard says 82, some receivers exceed it */
#define NMEA_MAX_FIELDS    (20U)

typedef enum {
    NMEA_SENTENCE_NONE = 0,      /**< still receiving             */
    NMEA_SENTENCE_RMC,
    NMEA_SENTENCE_GGA,
    NMEA_SENTENCE_VTG,
    NMEA_SENTENCE_GSA,
    NMEA_SENTENCE_GLL,
    NMEA_SENTENCE_UNKNOWN,       /**< valid checksum, type not parsed */
    NMEA_SENTENCE_BAD_CRC
} nmea_sentence_t;

typedef struct {
    bool     valid;              /**< RMC status 'A'                     */
    int32_t  lat_udeg;           /**< micro-degrees, north positive      */
    int32_t  lon_udeg;           /**< micro-degrees, east positive       */
    int32_t  alt_dm;             /**< altitude, decimetres (GGA)         */
    uint32_t speed_mmps;         /**< ground speed, mm/s                 */
    uint32_t course_cdeg;        /**< course over ground, 0.01 degree    */
    uint8_t  quality;            /**< GGA: 0 none, 1 GPS, 2 DGPS...      */
    uint8_t  satellites;
    uint16_t hdop_x100;
    uint8_t  hour, minute, second;
    uint16_t millis;
    uint8_t  day, month;
    uint16_t year;
} nmea_fix_t;

typedef struct {
    char       buf[NMEA_MAX_SENTENCE];
    uint32_t   len;
    bool       in_sentence;
    nmea_fix_t fix;
    uint32_t   sentences_ok;
    uint32_t   crc_errors;
    uint32_t   overflows;
} nmea_t;

void            nmea_init(nmea_t *n);
/** Returns the sentence type when a complete line was parsed. */
nmea_sentence_t nmea_feed(nmea_t *n, char c);
/** Parse one complete sentence ("$GPRMC,...*6A"), updates n->fix. */
nmea_sentence_t nmea_parse(nmea_t *n, const char *sentence);

/**
 * Build "$GPRMC,...*CS\r\n" from a fix (for tests, a GPS simulator,
 * or to forward a position to another device). Returns the length.
 */
uint32_t nmea_format_rmc(char *buf, uint32_t size, const nmea_fix_t *fix);

/* Field helpers (portable, unit-tested). */
/** "4807.038" + 'N' -> 48117300 micro-degrees (ddmm.mmmm format). */
bool nmea_parse_coord(const char *field, char hemisphere, int32_t *udeg);
/** Decimal field "12.345" -> integer scaled by 10^decimals (rounded). */
bool nmea_parse_fixed(const char *field, uint32_t decimals, int32_t *out);

#endif /* NMEA_H */

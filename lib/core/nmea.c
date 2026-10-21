/**
 * @file    nmea.c
 * @brief   NMEA parser (see nmea.h).
 */
#include "nmea.h"
#include "strutil.h"
#include "fmt.h"
#include "crc.h"

#define KNOT_MMPS_NUM     (514444U)   /* 1 knot = 0.514444 m/s */
#define KNOT_MMPS_DEN     (1000U)

void nmea_init(nmea_t *n)
{
    n->len          = 0U;
    n->in_sentence  = false;
    n->sentences_ok = 0U;
    n->crc_errors   = 0U;
    n->overflows    = 0U;
    n->fix.valid    = false;
    n->fix.lat_udeg = 0;
    n->fix.lon_udeg = 0;
    n->fix.quality  = 0U;
    n->fix.satellites = 0U;
}

/* ------------------------------ fields ---------------------------- */

bool nmea_parse_fixed(const char *field, uint32_t decimals, int32_t *out)
{
    int64_t  value    = 0;
    uint32_t frac     = 0U;
    bool     negative = false;
    bool     in_frac  = false;
    bool     digits   = false;
    bool     round_up = false;

    if (*field == '-') {
        negative = true;
        field++;
    }
    for (; *field != '\0'; field++) {
        if (*field == '.') {
            if (in_frac) {
                return false;
            }
            in_frac = true;
        } else if (chr_is_digit(*field)) {
            const int32_t d = *field - '0';
            digits = true;
            if (!in_frac || (frac < decimals)) {
                value = (value * 10) + d;
                if (in_frac) {
                    frac++;
                }
            } else if (frac == decimals) {
                round_up = (d >= 5);      /* first dropped digit rounds */
                frac++;
            } else {
                /* further digits ignored */
            }
        } else {
            return false;
        }
    }
    if (!digits) {
        return false;
    }
    for (; frac < decimals; frac++) {
        value *= 10;
    }
    if (round_up) {
        value++;
    }
    if (value > INT32_MAX) {
        return false;
    }
    *out = negative ? -(int32_t)value : (int32_t)value;
    return true;
}

bool nmea_parse_coord(const char *field, char hemisphere, int32_t *udeg)
{
    /* ddmm.mmmmm (lat) or dddmm.mmmmm (lon): minutes start 2 digits
     * before the dot */
    const char *dot = str_chr(field, '.');
    int32_t     degrees = 0;
    int32_t     minutes_e5;          /* minutes * 100000 */

    if ((dot == NULL) || ((dot - field) < 3)) {
        return false;
    }
    const char *min_start = dot - 2;
    for (const char *p = field; p < min_start; p++) {
        if (!chr_is_digit(*p)) {
            return false;
        }
        degrees = (degrees * 10) + (*p - '0');
    }
    if (!nmea_parse_fixed(min_start, 5U, &minutes_e5) || (minutes_e5 >= 6000000)) {
        return false;
    }
    /* udeg = deg * 1e6 + minutes / 60 * 1e6 = deg*1e6 + minutes_e5 * 10 / 60 */
    int64_t v = ((int64_t)degrees * 1000000) + ((((int64_t)minutes_e5 * 10) + 30) / 60);
    if ((hemisphere == 'S') || (hemisphere == 'W')) {
        v = -v;
    } else if ((hemisphere != 'N') && (hemisphere != 'E')) {
        return false;
    } else {
        /* positive */
    }
    *udeg = (int32_t)v;
    return true;
}

static bool parse_uint(const char *f, uint32_t *v)
{
    int32_t x;
    if (!nmea_parse_fixed(f, 0U, &x) || (x < 0)) {
        return false;
    }
    *v = (uint32_t)x;
    return true;
}

static void parse_time(nmea_fix_t *fix, const char *f)
{
    /* hhmmss.sss */
    if ((str_len(f) >= 6U) && chr_is_digit(f[0])) {
        fix->hour   = (uint8_t)(((f[0] - '0') * 10) + (f[1] - '0'));
        fix->minute = (uint8_t)(((f[2] - '0') * 10) + (f[3] - '0'));
        fix->second = (uint8_t)(((f[4] - '0') * 10) + (f[5] - '0'));
        int32_t ms = 0;
        fix->millis = ((f[6] == '.') && nmea_parse_fixed(&f[6], 3U, &ms)) ? (uint16_t)ms : 0U;
    }
}

static void parse_date(nmea_fix_t *fix, const char *f)
{
    /* ddmmyy */
    if ((str_len(f) == 6U) && chr_is_digit(f[0])) {
        fix->day   = (uint8_t)(((f[0] - '0') * 10) + (f[1] - '0'));
        fix->month = (uint8_t)(((f[2] - '0') * 10) + (f[3] - '0'));
        fix->year  = (uint16_t)(2000 + ((f[4] - '0') * 10) + (f[5] - '0'));
    }
}

static void set_speed_knots(nmea_fix_t *fix, const char *f)
{
    int32_t knots_e3;
    if (nmea_parse_fixed(f, 3U, &knots_e3) && (knots_e3 >= 0)) {
        fix->speed_mmps = (uint32_t)(((uint64_t)(uint32_t)knots_e3 * KNOT_MMPS_NUM) / (KNOT_MMPS_DEN * 1000U));
    }
}

static void set_course(nmea_fix_t *fix, const char *f)
{
    int32_t c;
    if (nmea_parse_fixed(f, 2U, &c) && (c >= 0)) {
        fix->course_cdeg = (uint32_t)c;
    }
}

/* ---------------------------- sentences --------------------------- */

static void parse_position(nmea_fix_t *fix, char *lat, char *ns, char *lon, char *ew)
{
    int32_t la;
    int32_t lo;
    if (nmea_parse_coord(lat, ns[0], &la) && nmea_parse_coord(lon, ew[0], &lo)) {
        fix->lat_udeg = la;
        fix->lon_udeg = lo;
    }
}

static nmea_sentence_t parse_fields(nmea_t *n, char *f[], uint32_t nf)
{
    const char *type = &f[0][2];          /* skip the talker ID "GP" */
    nmea_fix_t *fix  = &n->fix;

    if (str_eq(type, "RMC") && (nf >= 10U)) {
        /* $xxRMC,time,status,lat,N,lon,E,speed_kn,course,date,... */
        parse_time(fix, f[1]);
        fix->valid = (f[2][0] == 'A');
        if (fix->valid) {
            parse_position(fix, f[3], f[4], f[5], f[6]);
            set_speed_knots(fix, f[7]);
            set_course(fix, f[8]);
        }
        parse_date(fix, f[9]);
        return NMEA_SENTENCE_RMC;
    }
    if (str_eq(type, "GGA") && (nf >= 10U)) {
        /* $xxGGA,time,lat,N,lon,E,quality,sats,hdop,alt,M,... */
        uint32_t q = 0U;
        uint32_t s = 0U;
        int32_t  h;
        int32_t  alt;
        parse_time(fix, f[1]);
        if (parse_uint(f[6], &q)) {
            fix->quality = (uint8_t)q;
        }
        if (fix->quality > 0U) {
            parse_position(fix, f[2], f[3], f[4], f[5]);
        }
        if (parse_uint(f[7], &s)) {
            fix->satellites = (uint8_t)s;
        }
        if (nmea_parse_fixed(f[8], 2U, &h) && (h >= 0)) {
            fix->hdop_x100 = (uint16_t)((h > 0xFFFF) ? 0xFFFF : h);
        }
        if (nmea_parse_fixed(f[9], 1U, &alt)) {
            fix->alt_dm = alt;
        }
        return NMEA_SENTENCE_GGA;
    }
    if (str_eq(type, "VTG") && (nf >= 8U)) {
        /* $xxVTG,course_t,T,course_m,M,speed_kn,N,speed_kmh,K */
        set_course(fix, f[1]);
        set_speed_knots(fix, f[5]);
        return NMEA_SENTENCE_VTG;
    }
    if (str_eq(type, "GLL") && (nf >= 7U)) {
        /* $xxGLL,lat,N,lon,E,time,status */
        if (f[6][0] == 'A') {
            parse_position(fix, f[1], f[2], f[3], f[4]);
        }
        parse_time(fix, f[5]);
        return NMEA_SENTENCE_GLL;
    }
    if (str_eq(type, "GSA") && (nf >= 17U)) {
        int32_t h;
        if (nmea_parse_fixed(f[16], 2U, &h) && (h >= 0)) {
            fix->hdop_x100 = (uint16_t)((h > 0xFFFF) ? 0xFFFF : h);
        }
        return NMEA_SENTENCE_GSA;
    }
    return NMEA_SENTENCE_UNKNOWN;
}

nmea_sentence_t nmea_parse(nmea_t *n, const char *sentence)
{
    char     line[NMEA_MAX_SENTENCE];
    char    *f[NMEA_MAX_FIELDS];

    if (sentence[0] != '$') {
        return NMEA_SENTENCE_NONE;
    }
    (void)str_copy(line, &sentence[1], sizeof(line));
    char *star = (char *)str_chr(line, '*');
    if (star == NULL) {
        n->crc_errors++;
        return NMEA_SENTENCE_BAD_CRC;    /* checksum is mandatory for us */
    }
    /* XOR of every char between '$' and '*' */
    uint8_t crc = 0U;
    for (const char *p = line; p < star; p++) {
        crc ^= (uint8_t)*p;
    }
    const int hi = chr_hex_value(star[1]);
    const int lo = chr_hex_value(star[2]);
    if ((hi < 0) || (lo < 0) || (crc != (uint8_t)((hi << 4) | lo))) {
        n->crc_errors++;
        return NMEA_SENTENCE_BAD_CRC;
    }
    *star = '\0';
    const uint32_t nf = str_split(line, ',', f, NMEA_MAX_FIELDS);
    if ((nf == 0U) || (str_len(f[0]) != 5U)) {
        return NMEA_SENTENCE_UNKNOWN;
    }
    n->sentences_ok++;
    return parse_fields(n, f, nf);
}

nmea_sentence_t nmea_feed(nmea_t *n, char c)
{
    if (c == '$') {
        n->in_sentence = true;            /* resynchronise on every '$' */
        n->len         = 0U;
    }
    if (!n->in_sentence) {
        return NMEA_SENTENCE_NONE;
    }
    if ((c == '\r') || (c == '\n')) {
        n->in_sentence = false;
        n->buf[n->len] = '\0';
        return nmea_parse(n, n->buf);
    }
    if (n->len >= (NMEA_MAX_SENTENCE - 1U)) {
        n->in_sentence = false;           /* garbage: drop it */
        n->overflows++;
        return NMEA_SENTENCE_NONE;
    }
    n->buf[n->len++] = c;
    return NMEA_SENTENCE_NONE;
}

/* ------------------------------ builder --------------------------- */

/* micro-degrees -> "ddmm.mmmmm" (lat: 2 degree digits, lon: 3) */
static void coord_text(char *buf, uint32_t size, int32_t udeg, uint32_t deg_digits)
{
    const uint32_t mag        = (udeg < 0) ? (0U - (uint32_t)udeg) : (uint32_t)udeg;
    const uint32_t degrees    = mag / 1000000U;
    /* minutes * 1e5 = fraction * 60 * 1e5 / 1e6 = fraction * 6, rounded */
    const uint32_t minutes_e5 = (((mag % 1000000U) * 60U) + 5U) / 10U;
    (void)fmt_snprintf(buf, size, "%0*lu%02lu.%05lu", (int)deg_digits, (unsigned long)degrees,
                       (unsigned long)(minutes_e5 / 100000U), (unsigned long)(minutes_e5 % 100000U));
}

uint32_t nmea_format_rmc(char *buf, uint32_t size, const nmea_fix_t *fix)
{
    char     lat[16];
    char     lon[16];
    char     body[NMEA_MAX_SENTENCE];
    /* knots * 1000 = mm/s * 1000 / 514.444 */
    const uint32_t knots_e3 = (uint32_t)(((uint64_t)fix->speed_mmps * 1000000ULL) / 514444ULL);

    coord_text(lat, sizeof(lat), fix->lat_udeg, 2U);
    coord_text(lon, sizeof(lon), fix->lon_udeg, 3U);
    (void)fmt_snprintf(body, sizeof(body), "GPRMC,%02u%02u%02u.%02u,%c,%s,%c,%s,%c,%lu.%03lu,%lu.%02lu,%02u%02u%02u,,,A",
                       (unsigned)fix->hour, (unsigned)fix->minute, (unsigned)fix->second,
                       (unsigned)(fix->millis / 10U), fix->valid ? 'A' : 'V',
                       lat, (fix->lat_udeg < 0) ? 'S' : 'N', lon, (fix->lon_udeg < 0) ? 'W' : 'E',
                       (unsigned long)(knots_e3 / 1000U), (unsigned long)(knots_e3 % 1000U),
                       (unsigned long)(fix->course_cdeg / 100U), (unsigned long)(fix->course_cdeg % 100U),
                       (unsigned)fix->day, (unsigned)fix->month, (unsigned)(fix->year % 100U));
    const int n = fmt_snprintf(buf, size, "$%s*%02X\r\n", body, (unsigned)checksum_nmea(body));
    return (n < 0) ? 0U : (uint32_t)n;
}

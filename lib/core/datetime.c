/**
 * @file    datetime.c
 * @brief   Calendar conversions (see datetime.h).
 */
#include "datetime.h"
#include "fmt.h"
#include "strutil.h"

#define SECONDS_PER_DAY   (86400UL)
#define UNIX_MAX_YEAR     (2105U)

bool dt_is_leap(uint32_t year)
{
    return ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
}

uint8_t dt_days_in_month(uint32_t year, uint32_t month)
{
    static const uint8_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if ((month < 1U) || (month > 12U)) {
        return 0U;
    }
    return (uint8_t)(days[month - 1U] + (((month == 2U) && dt_is_leap(year)) ? 1U : 0U));
}

int32_t dt_days_from_civil(int32_t year, uint32_t month, uint32_t day)
{
    /* shift the year so it starts in March: Feb 29 is the last day */
    year -= (month <= 2U) ? 1 : 0;
    const int32_t  era = ((year >= 0) ? year : (year - 399)) / 400;
    const uint32_t yoe = (uint32_t)(year - (era * 400));                       /* 0..399   */
    const uint32_t mp  = (month > 2U) ? (month - 3U) : (month + 9U);           /* Mar = 0  */
    const uint32_t doy = (((153U * mp) + 2U) / 5U) + day - 1U;                 /* 0..365   */
    const uint32_t doe = (yoe * 365U) + (yoe / 4U) - (yoe / 100U) + doy;       /* 0..146096 */
    return (era * 146097) + (int32_t)doe - 719468;
}

void dt_from_unix(uint32_t t, datetime_t *dt)
{
    const uint32_t days = t / SECONDS_PER_DAY;
    const uint32_t secs = t % SECONDS_PER_DAY;

    dt->hour    = (uint8_t)(secs / 3600U);
    dt->minute  = (uint8_t)((secs / 60U) % 60U);
    dt->second  = (uint8_t)(secs % 60U);
    dt->weekday = (uint8_t)((days + 4U) % 7U);     /* 1970-01-01 was a Thursday */

    /* civil_from_days (inverse of the function above) */
    const uint32_t z   = days + 719468U;
    const uint32_t era = z / 146097U;
    const uint32_t doe = z - (era * 146097U);
    const uint32_t yoe = (doe - (doe / 1460U) + (doe / 36524U) - (doe / 146096U)) / 365U;
    const uint32_t doy = doe - ((365U * yoe) + (yoe / 4U) - (yoe / 100U));
    const uint32_t mp  = ((5U * doy) + 2U) / 153U;
    const uint32_t d   = doy - (((153U * mp) + 2U) / 5U) + 1U;
    const uint32_t m   = (mp < 10U) ? (mp + 3U) : (mp - 9U);
    const uint32_t y   = (yoe + (era * 400U)) + ((m <= 2U) ? 1U : 0U);

    dt->year  = (uint16_t)y;
    dt->month = (uint8_t)m;
    dt->day   = (uint8_t)d;
}

bool dt_is_valid(const datetime_t *dt)
{
    return (dt->year >= 1970U) && (dt->year <= UNIX_MAX_YEAR) &&
           (dt->month >= 1U) && (dt->month <= 12U) &&
           (dt->day >= 1U) && (dt->day <= dt_days_in_month(dt->year, dt->month)) &&
           (dt->hour < 24U) && (dt->minute < 60U) && (dt->second < 60U);
}

bool dt_to_unix(const datetime_t *dt, uint32_t *t)
{
    if (!dt_is_valid(dt)) {
        return false;
    }
    const int32_t  days = dt_days_from_civil((int32_t)dt->year, dt->month, dt->day);
    const uint64_t s    = ((uint64_t)(uint32_t)days * SECONDS_PER_DAY) +
                          ((uint32_t)dt->hour * 3600U) + ((uint32_t)dt->minute * 60U) + dt->second;
    if (s > 0xFFFFFFFFULL) {
        return false;
    }
    *t = (uint32_t)s;
    return true;
}

size_t dt_format(const datetime_t *dt, char *buf, size_t size)
{
    const int n = fmt_snprintf(buf, size, "%04u-%02u-%02u %02u:%02u:%02u",
                               (unsigned)dt->year, (unsigned)dt->month, (unsigned)dt->day,
                               (unsigned)dt->hour, (unsigned)dt->minute, (unsigned)dt->second);
    return (n < 0) ? 0U : (size_t)n;
}

/* Parse exactly @p digits decimal digits. */
static bool parse_fixed(const char **p, uint32_t digits, uint32_t *out)
{
    uint32_t v = 0U;
    for (uint32_t i = 0U; i < digits; i++) {
        if (!chr_is_digit(**p)) {
            return false;
        }
        v = (v * 10U) + (uint32_t)(**p - '0');
        (*p)++;
    }
    *out = v;
    return true;
}

static bool parse_date(const char **p, datetime_t *dt)
{
    uint32_t y;
    uint32_t m;
    uint32_t d;
    if (!parse_fixed(p, 4U, &y) || (**p != '-')) { return false; }
    (*p)++;
    if (!parse_fixed(p, 2U, &m) || (**p != '-')) { return false; }
    (*p)++;
    if (!parse_fixed(p, 2U, &d)) { return false; }
    dt->year  = (uint16_t)y;
    dt->month = (uint8_t)m;
    dt->day   = (uint8_t)d;
    return true;
}

static bool parse_time(const char **p, datetime_t *dt)
{
    uint32_t h;
    uint32_t mi;
    uint32_t s = 0U;
    if (!parse_fixed(p, 2U, &h) || (**p != ':')) { return false; }
    (*p)++;
    if (!parse_fixed(p, 2U, &mi)) { return false; }
    if (**p == ':') {
        (*p)++;
        if (!parse_fixed(p, 2U, &s)) { return false; }
    }
    dt->hour   = (uint8_t)h;
    dt->minute = (uint8_t)mi;
    dt->second = (uint8_t)s;
    return true;
}

bool dt_parse(const char *s, datetime_t *dt)
{
    datetime_t  tmp = *dt;
    const char *p   = s;

    if ((str_len(s) >= 10U) && (s[4] == '-')) {
        if (!parse_date(&p, &tmp)) {
            return false;
        }
        if ((*p == ' ') || (*p == 'T')) {
            p++;
            if (!parse_time(&p, &tmp)) {
                return false;
            }
        }
    } else if (!parse_time(&p, &tmp)) {
        return false;
    } else {
        /* time only */
    }
    if ((*p != '\0') || !dt_is_valid(&tmp)) {
        return false;
    }
    uint32_t t;
    (void)dt_to_unix(&tmp, &t);
    dt_from_unix(t, &tmp);        /* recompute the weekday */
    *dt = tmp;
    return true;
}

const char *dt_weekday_name(uint8_t weekday)
{
    static const char *const names[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    return (weekday < 7U) ? names[weekday] : "?";
}

size_t dt_format_duration(uint32_t seconds, char *buf, size_t size)
{
    const uint32_t days = seconds / SECONDS_PER_DAY;
    const uint32_t rem  = seconds % SECONDS_PER_DAY;
    int n;
    if (days > 0U) {
        n = fmt_snprintf(buf, size, "%lud %02u:%02u:%02u", (unsigned long)days,
                         (unsigned)(rem / 3600U), (unsigned)((rem / 60U) % 60U), (unsigned)(rem % 60U));
    } else {
        n = fmt_snprintf(buf, size, "%02u:%02u:%02u",
                         (unsigned)(rem / 3600U), (unsigned)((rem / 60U) % 60U), (unsigned)(rem % 60U));
    }
    return (n < 0) ? 0U : (size_t)n;
}

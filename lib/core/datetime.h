/**
 * @file    datetime.h
 * @brief   Calendar <-> Unix time (seconds since 1970-01-01 UTC),
 *          integer only, valid for years 1970..2105 (uint32_t).
 *
 * The F1 RTC is only a seconds counter: this module turns it into a
 * date and back, so "date 2026-10-16 08:30:00" works in the CLI.
 * Algorithm: H. Hinnant's days_from_civil (no loops, no tables).
 */
#ifndef DATETIME_H
#define DATETIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint16_t year;     /**< 1970..2105        */
    uint8_t  month;    /**< 1..12             */
    uint8_t  day;      /**< 1..31             */
    uint8_t  hour;     /**< 0..23             */
    uint8_t  minute;   /**< 0..59             */
    uint8_t  second;   /**< 0..59             */
    uint8_t  weekday;  /**< 0 = Sunday .. 6   */
} datetime_t;

bool     dt_is_leap(uint32_t year);
uint8_t  dt_days_in_month(uint32_t year, uint32_t month);
/** Days since 1970-01-01 (can be negative before 1970). */
int32_t  dt_days_from_civil(int32_t year, uint32_t month, uint32_t day);
void     dt_from_unix(uint32_t t, datetime_t *dt);
/** Returns false if a field is out of range. */
bool     dt_to_unix(const datetime_t *dt, uint32_t *t);
bool     dt_is_valid(const datetime_t *dt);
/** "YYYY-MM-DD HH:MM:SS" into buf (>= 20 bytes). */
size_t   dt_format(const datetime_t *dt, char *buf, size_t size);
/** Parse "YYYY-MM-DD", "HH:MM[:SS]" or both separated by ' ' or 'T'.
 *  Missing parts keep their value in @p dt. */
bool     dt_parse(const char *s, datetime_t *dt);
const char *dt_weekday_name(uint8_t weekday);   /**< "Mon" ... */
/** "3d 04:05:06" style duration. */
size_t   dt_format_duration(uint32_t seconds, char *buf, size_t size);

#endif /* DATETIME_H */

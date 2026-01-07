/**
 * Timezone Handling
 *
 * Supports multiple timezone rules for different locations.
 * Rule selected via TZ_RULE in location_config.h
 */

#include "timezone.h"
#include "location_config.h"
#include <string.h>

// Days in each month (non-leap year)
static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Check if year is a leap year
static int is_leap_year(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

// Days in a given year
static int days_in_year(int year) {
    return is_leap_year(year) ? 366 : 365;
}

/**
 * Convert 64-bit Unix timestamp to struct tm.
 * Replaces gmtime() with 64-bit safe implementation.
 */
void time_to_tm(time64_t t, struct tm *result) {
    int64_t days = t / 86400;
    int64_t rem = t % 86400;

    // Handle negative timestamps (before 1970)
    if (rem < 0) {
        rem += 86400;
        days--;
    }

    result->tm_hour = (int)(rem / 3600);
    rem %= 3600;
    result->tm_min = (int)(rem / 60);
    result->tm_sec = (int)(rem % 60);

    // Day of week: Jan 1, 1970 was Thursday (4)
    result->tm_wday = (int)((days + 4) % 7);
    if (result->tm_wday < 0) result->tm_wday += 7;

    // Find year
    int year = 1970;
    if (days >= 0) {
        while (days >= days_in_year(year)) {
            days -= days_in_year(year);
            year++;
        }
    } else {
        while (days < 0) {
            year--;
            days += days_in_year(year);
        }
    }
    result->tm_year = year - 1900;
    result->tm_yday = (int)days;

    // Find month and day
    int leap = is_leap_year(year);
    int month = 0;
    while (month < 11) {
        int dim = days_in_month[month];
        if (month == 1 && leap) dim++;  // February in leap year
        if (days < dim) break;
        days -= dim;
        month++;
    }
    result->tm_mon = month;
    result->tm_mday = (int)days + 1;
    result->tm_isdst = 0;
}

// Calculate day of week (0=Sunday) for a given date using Zeller's formula
static int day_of_week(int year, int month, int day) {
    if (month < 3) {
        month += 12;
        year--;
    }
    int k = year % 100;
    int j = year / 100;
    int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    // Convert from Zeller (0=Sat) to standard (0=Sun)
    return ((h + 6) % 7);
}

// ============================================================================
// DST Rules by Region
// ============================================================================

// Find nth occurrence of weekday in month (n=1,2,3,4,5 where 5=last)
// Returns day of month (1-31)
static int find_nth_weekday(int year, int month, int weekday, int n) {
    // Get day of week for first of month
    int first_dow = day_of_week(year, month, 1);

    // Days until first occurrence of target weekday
    int days_to_first = (weekday - first_dow + 7) % 7;
    int first_occurrence = 1 + days_to_first;

    if (n == 5) {
        // "Last" - find last occurrence
        int dim = days_in_month[month - 1];
        if (month == 2 && is_leap_year(year)) dim++;
        int last_occurrence = first_occurrence;
        while (last_occurrence + 7 <= dim) {
            last_occurrence += 7;
        }
        return last_occurrence;
    }

    return first_occurrence + (n - 1) * 7;
}

// UK/EU: Last Sunday of March/October at 01:00 UTC
static int is_dst_eu(time64_t utc_time, int march_to_oct) {
    struct tm tm_utc;
    time_to_tm(utc_time, &tm_utc);
    int year = tm_utc.tm_year + 1900;
    int month = tm_utc.tm_mon + 1;
    int day = tm_utc.tm_mday;
    int hour = tm_utc.tm_hour;

    if (month < 3 || month > 10) return 0;
    if (month > 3 && month < 10) return 1;

    if (month == 3) {
        int last_sunday = find_nth_weekday(year, 3, 0, 5);  // 0=Sunday, 5=last
        if (day < last_sunday) return 0;
        if (day > last_sunday) return 1;
        return (hour >= 1);
    }

    if (month == 10) {
        int last_sunday = find_nth_weekday(year, 10, 0, 5);
        if (day < last_sunday) return 1;
        if (day > last_sunday) return 0;
        return (hour < 1);
    }

    return 0;
}

// US: 2nd Sunday of March at 02:00 local, 1st Sunday of November at 02:00 local
// We check in UTC, accounting for standard time offset
static int is_dst_us(time64_t utc_time, int standard_offset_hours) {
    // Convert to local standard time for checking
    time64_t local_std = utc_time + (int64_t)standard_offset_hours * 3600;
    struct tm tm_local;
    time_to_tm(local_std, &tm_local);
    int year = tm_local.tm_year + 1900;
    int month = tm_local.tm_mon + 1;
    int day = tm_local.tm_mday;
    int hour = tm_local.tm_hour;

    if (month < 3 || month > 11) return 0;
    if (month > 3 && month < 11) return 1;

    if (month == 3) {
        int second_sunday = find_nth_weekday(year, 3, 0, 2);  // 0=Sunday, 2=second
        if (day < second_sunday) return 0;
        if (day > second_sunday) return 1;
        return (hour >= 2);
    }

    if (month == 11) {
        int first_sunday = find_nth_weekday(year, 11, 0, 1);  // 0=Sunday, 1=first
        if (day < first_sunday) return 1;
        if (day > first_sunday) return 0;
        return (hour < 2);
    }

    return 0;
}

// Australia Eastern: 1st Sunday of October at 02:00 local, 1st Sunday of April at 03:00 local
static int is_dst_au_eastern(time64_t utc_time) {
    // AEST is UTC+10
    time64_t local_std = utc_time + 10 * 3600;
    struct tm tm_local;
    time_to_tm(local_std, &tm_local);
    int year = tm_local.tm_year + 1900;
    int month = tm_local.tm_mon + 1;
    int day = tm_local.tm_mday;
    int hour = tm_local.tm_hour;

    // Southern hemisphere: DST from October to April
    if (month > 4 && month < 10) return 0;  // Winter (no DST)
    if (month > 10 || month < 4) return 1;  // Summer (DST)

    if (month == 10) {
        int first_sunday = find_nth_weekday(year, 10, 0, 1);
        if (day < first_sunday) return 0;
        if (day > first_sunday) return 1;
        return (hour >= 2);
    }

    if (month == 4) {
        int first_sunday = find_nth_weekday(year, 4, 0, 1);
        if (day < first_sunday) return 1;
        if (day > first_sunday) return 0;
        return (hour < 3);  // Clocks go back at 03:00 AEDT -> 02:00 AEST
    }

    return 0;
}

// New Zealand: Last Sunday of September at 02:00 local, 1st Sunday of April at 03:00 local
static int is_dst_nz(time64_t utc_time) {
    // NZST is UTC+12
    time64_t local_std = utc_time + 12 * 3600;
    struct tm tm_local;
    time_to_tm(local_std, &tm_local);
    int year = tm_local.tm_year + 1900;
    int month = tm_local.tm_mon + 1;
    int day = tm_local.tm_mday;
    int hour = tm_local.tm_hour;

    // Southern hemisphere: DST from late September to early April
    if (month > 4 && month < 9) return 0;  // Winter
    if (month > 9 || month < 4) return 1;  // Summer

    if (month == 9) {
        int last_sunday = find_nth_weekday(year, 9, 0, 5);
        if (day < last_sunday) return 0;
        if (day > last_sunday) return 1;
        return (hour >= 2);
    }

    if (month == 4) {
        int first_sunday = find_nth_weekday(year, 4, 0, 1);
        if (day < first_sunday) return 1;
        if (day > first_sunday) return 0;
        return (hour < 3);
    }

    return 0;
}

// ============================================================================
// Generic DST Check
// ============================================================================

/**
 * Check if DST is in effect for configured timezone.
 * Returns 1 if DST, 0 if standard time.
 */
int is_dst(time64_t utc_time) {
    switch (TZ_RULE) {
        case TZ_RULE_UTC:
        case TZ_RULE_US_HAWAII:
        case TZ_RULE_AU_WESTERN:
            return 0;  // No DST

        case TZ_RULE_UK:
        case TZ_RULE_EU_CENTRAL:
        case TZ_RULE_EU_EASTERN:
            return is_dst_eu(utc_time, 1);

        case TZ_RULE_US_EASTERN:
            return is_dst_us(utc_time, -5);
        case TZ_RULE_US_CENTRAL:
            return is_dst_us(utc_time, -6);
        case TZ_RULE_US_MOUNTAIN:
            return is_dst_us(utc_time, -7);
        case TZ_RULE_US_PACIFIC:
            return is_dst_us(utc_time, -8);
        case TZ_RULE_US_ALASKA:
            return is_dst_us(utc_time, -9);

        case TZ_RULE_AU_EASTERN:
            return is_dst_au_eastern(utc_time);

        case TZ_RULE_NZ:
            return is_dst_nz(utc_time);

        default:
            return 0;
    }
}

// Legacy function for backward compatibility
int is_bst(time64_t utc_time) {
    return is_dst(utc_time);
}

/**
 * Get UTC offset in seconds for configured timezone at given time.
 */
int get_utc_offset(time64_t utc_time) {
    int dst = is_dst(utc_time);
    int offset_hours = dst ? TZ_OFFSET_SUMMER : TZ_OFFSET_WINTER;
    return offset_hours * 3600;
}

/**
 * Convert UTC time to local time for configured timezone.
 */
struct tm* utc_to_local(time64_t utc_time) {
    static struct tm result;
    int offset = get_utc_offset(utc_time);
    time64_t local_time = utc_time + offset;
    time_to_tm(local_time, &result);
    return &result;
}

// Legacy function name for backward compatibility
struct tm* utc_to_margate(time64_t utc_time) {
    return utc_to_local(utc_time);
}

// Manual UTC to time64_t conversion (embedded systems don't have timegm)
static time64_t my_timegm(struct tm *tm) {
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;
    int min = tm->tm_min;
    int sec = tm->tm_sec;

    // Count leap years from 1970 to year-1
    int y = year - 1;
    int leap_years_before = (y / 4) - (y / 100) + (y / 400);
    int leap_years_before_1970 = (1969 / 4) - (1969 / 100) + (1969 / 400);
    int leap_years = leap_years_before - leap_years_before_1970;

    int64_t days = (int64_t)(year - 1970) * 365 + leap_years;

    // Days from months in current year
    static const int days_before_month_arr[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    days += days_before_month_arr[month - 1];

    // Add leap day if this is a leap year and we're past February
    if (is_leap_year(year) && month > 2) {
        days++;
    }

    days += day - 1;  // Day of month (1-based)

    return (time64_t)(days * 86400 + hour * 3600 + min * 60 + sec);
}

/**
 * Parse local time and convert to UTC.
 */
time64_t parse_local_time(int year, int month, int day, int hour, int minute) {
    struct tm tm_input = {0};
    tm_input.tm_year = year - 1900;
    tm_input.tm_mon = month - 1;
    tm_input.tm_mday = day;
    tm_input.tm_hour = hour;
    tm_input.tm_min = minute;
    tm_input.tm_isdst = 0;

    // First, assume standard time
    time64_t utc_guess = my_timegm(&tm_input) - TZ_OFFSET_WINTER * 3600;

    // Check if DST would apply
    if (is_dst(utc_guess)) {
        utc_guess = my_timegm(&tm_input) - TZ_OFFSET_SUMMER * 3600;
    }

    return utc_guess;
}

// Legacy function name for backward compatibility
time64_t parse_margate_time(int year, int month, int day, int hour, int minute) {
    return parse_local_time(year, month, day, hour, minute);
}

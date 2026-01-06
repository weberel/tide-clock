/**
 * UK Timezone Handling (Margate = GMT/BST)
 */

#include "timezone.h"
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

int is_bst(time64_t utc_time) {
    struct tm tm_utc;
    time_to_tm(utc_time, &tm_utc);
    int year = tm_utc.tm_year + 1900;
    int month = tm_utc.tm_mon + 1;
    int day = tm_utc.tm_mday;
    int hour = tm_utc.tm_hour;

    if (month < 3 || month > 10) return 0;
    if (month > 3 && month < 10) return 1;

    if (month == 3) {
        // Find last Sunday of March
        int wday_31 = day_of_week(year, 3, 31);
        int last_sunday = 31 - wday_31;

        if (day < last_sunday) return 0;
        if (day > last_sunday) return 1;
        return (hour >= 1);
    }

    if (month == 10) {
        // Find last Sunday of October
        int wday_31 = day_of_week(year, 10, 31);
        int last_sunday = 31 - wday_31;

        if (day < last_sunday) return 1;
        if (day > last_sunday) return 0;
        return (hour < 1);
    }

    return 0;
}

struct tm* utc_to_margate(time64_t utc_time) {
    static struct tm result;
    int bst = is_bst(utc_time);
    time64_t local_time = utc_time + (bst ? 3600 : 0);
    time_to_tm(local_time, &result);
    return &result;
}

// Manual UTC to time64_t conversion (embedded systems don't have timegm)
// This correctly converts a UTC struct tm to Unix timestamp
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

time64_t parse_margate_time(int year, int month, int day, int hour, int minute) {
    struct tm tm_input = {0};
    tm_input.tm_year = year - 1900;
    tm_input.tm_mon = month - 1;
    tm_input.tm_mday = day;
    tm_input.tm_hour = hour;
    tm_input.tm_min = minute;
    tm_input.tm_isdst = 0;

    time64_t utc_guess = my_timegm(&tm_input);
    int would_be_bst = is_bst(utc_guess - 3600);

    if (would_be_bst) {
        utc_guess -= 3600;
    }

    return utc_guess;
}

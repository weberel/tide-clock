/**
 * Verify astronomical calculations against reference data.
 * Uses EXACT firmware code from astro.c
 *
 * Moon phase: 0=new (dark), 0.5=full (bright)
 * Sunrise/Sunset: Local time (GMT/BST)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

typedef int64_t time64_t;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// EXACT COPY FROM FIRMWARE: timezone.c (BST detection)
// ============================================================================

static int is_bst(time64_t t) {
    struct tm *tm = gmtime((time_t*)&t);
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;

    if (month < 3 || month > 10) return 0;
    if (month > 3 && month < 10) return 1;

    int last_sunday;
    if (month == 3) {
        last_sunday = 31 - ((5 * year / 4 + 4) % 7);
        if (day < last_sunday) return 0;
        if (day > last_sunday) return 1;
        return hour >= 1;
    } else {
        last_sunday = 31 - ((5 * year / 4 + 1) % 7);
        if (day < last_sunday) return 1;
        if (day > last_sunday) return 0;
        return hour < 1;
    }
}

static void time_to_tm(time64_t t, struct tm *tm) {
    time_t tt = (time_t)t;
    struct tm *tmp = gmtime(&tt);
    *tm = *tmp;
}

// ============================================================================
// EXACT COPY FROM FIRMWARE: astro.c
// ============================================================================

static int calc_day_of_year(int year, int month, int day) {
    static const int days_before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int doy = days_before_month[month - 1] + day;
    int is_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (is_leap && month > 2) {
        doy++;
    }
    return doy;
}

static double moon_phase_jde(double k) {
    double T = k / 1236.85;
    double T2 = T * T;
    double T3 = T2 * T;
    double T4 = T3 * T;

    double JDE = 2451550.09766 + 29.530588861*k
                 + 0.00015437*T2 - 0.000000150*T3 + 0.00000000073*T4;

    double M = fmod(2.5534 + 29.10535670*k - 0.0000014*T2, 360.0) * M_PI / 180.0;
    double Mp = fmod(201.5643 + 385.81693528*k + 0.0107582*T2, 360.0) * M_PI / 180.0;
    double F = fmod(160.7108 + 390.67050284*k - 0.0016118*T2, 360.0) * M_PI / 180.0;
    double Om = fmod(124.7746 - 1.56375588*k + 0.0020672*T2, 360.0) * M_PI / 180.0;
    double E = 1 - 0.002516*T - 0.0000074*T2;

    double corr = -0.40720*sin(Mp) + 0.17241*E*sin(M) + 0.01608*sin(2*Mp)
                + 0.01039*sin(2*F) + 0.00739*E*sin(Mp-M) - 0.00514*E*sin(Mp+M)
                + 0.00208*E*E*sin(2*M) - 0.00111*sin(Mp-2*F) - 0.00057*sin(Mp+2*F)
                + 0.00056*E*sin(2*Mp+M) - 0.00042*sin(3*Mp) + 0.00042*E*sin(M+2*F)
                + 0.00038*E*sin(M-2*F) - 0.00024*E*sin(2*Mp-M) - 0.00017*sin(Om);

    return JDE + corr;
}

static time64_t jde_to_time64(double JDE) {
    return (time64_t)((JDE - 2440587.5) * 86400.0);
}

float calculate_moon_phase(time64_t dt) {
    struct tm tm;
    time_to_tm(dt, &tm);
    int year = tm.tm_year + 1900;
    int month = tm.tm_mon + 1;
    int day = tm.tm_mday;
    double hour = tm.tm_hour + tm.tm_min/60.0;

    if (month <= 2) { year--; month += 12; }
    int A = year / 100;
    int B = 2 - A + A/4;
    double JD = (int)(365.25 * (year + 4716)) + (int)(30.6001 * (month + 1))
                + day + hour/24.0 + B - 1524.5;

    double T = (JD - 2451545.0) / 36525.0;
    double D = fmod(297.8501921 + 445267.1114034*T, 360.0);
    double M = fmod(357.5291092 + 35999.0502909*T, 360.0) * M_PI/180;
    double Mp = fmod(134.9633964 + 477198.8675055*T, 360.0) * M_PI/180;

    double i = 180.0 - D - 6.289*sin(Mp) + 2.100*sin(M) - 1.274*sin(2*D*M_PI/180 - Mp);
    i = fmod(i, 360.0);
    if (i < 0) i += 360.0;

    // Convert to 0=new, 0.5=full convention
    double phase = 0.5 - (i / 360.0);
    if (phase < 0) phase += 1.0;
    return (float)phase;
}

void find_next_full_new_moon(time64_t dt, time64_t *next_full, time64_t *next_new) {
    struct tm tm;
    time_to_tm(dt, &tm);
    int y = tm.tm_year + 1900;
    int m = tm.tm_mon + 1;
    int d = tm.tm_mday;
    int doy = calc_day_of_year(y, m, d);
    double year = y + (doy - 1) / 365.25;
    double k_approx = (year - 2000.0) * 12.3685;
    int k_base = (int)floor(k_approx);

    *next_full = 0;
    *next_new = 0;

    // Find moon phases that are at least tomorrow (not today)
    // This avoids showing "today" as "next" when the event just happened
    time64_t tomorrow_start = dt + 86400 - (tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);

    for (int i = -1; i <= 3; i++) {
        int k = k_base + i;
        time64_t t_new = jde_to_time64(moon_phase_jde((double)k));
        if (*next_new == 0 && t_new >= tomorrow_start) *next_new = t_new;

        time64_t t_full = jde_to_time64(moon_phase_jde(k + 0.5));
        if (*next_full == 0 && t_full >= tomorrow_start) *next_full = t_full;
    }
}

#define MARGATE_LAT  51.3813f
#define MARGATE_LON  1.3862f

void calculate_sunrise_sunset(time64_t dt, int *sunrise_hour, int *sunrise_min,
                              int *sunset_hour, int *sunset_min) {
    struct tm tm_info;
    time_to_tm(dt, &tm_info);
    int day_of_year = calc_day_of_year(tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday);

    float gamma = 2 * M_PI / 365 * (day_of_year - 1);

    float eqtime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma)
                   - 0.014615 * cos(2*gamma) - 0.040849 * sin(2*gamma));

    float decl = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma)
                 - 0.006758 * cos(2*gamma) + 0.000907 * sin(2*gamma)
                 - 0.002697 * cos(3*gamma) + 0.00148 * sin(3*gamma);

    float lat_rad = MARGATE_LAT * M_PI / 180.0;
    float zenith = 90.833 * M_PI / 180.0;
    float cos_hour_angle = (cos(zenith) / (cos(lat_rad) * cos(decl))) - tan(lat_rad) * tan(decl);
    cos_hour_angle = fmax(-1, fmin(1, cos_hour_angle));

    float hour_angle = acos(cos_hour_angle) * 180.0 / M_PI;

    float sunrise_utc_min = 720 - 4 * (MARGATE_LON + hour_angle) - eqtime;
    float sunset_utc_min = 720 - 4 * (MARGATE_LON - hour_angle) - eqtime;

    float sunrise_utc = sunrise_utc_min / 60.0;
    float sunset_utc = sunset_utc_min / 60.0;

    // Add BST offset if applicable
    int bst = is_bst(dt);
    if (bst) {
        sunrise_utc += 1.0f;
        sunset_utc += 1.0f;
    }

    *sunrise_hour = (int)sunrise_utc;
    *sunrise_min = (int)((sunrise_utc - *sunrise_hour) * 60);
    *sunset_hour = (int)sunset_utc;
    *sunset_min = (int)((sunset_utc - *sunset_hour) * 60);
}

// ============================================================================
// Main - output data for verification
// ============================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s YYYY-MM-DD\n", argv[0]);
        printf("Outputs: moon_phase, sunrise, sunset, next_full, next_new\n");
        return 1;
    }

    int year, month, day;
    if (sscanf(argv[1], "%d-%d-%d", &year, &month, &day) != 3) {
        printf("Invalid date format\n");
        return 1;
    }

    // Create UTC timestamp for noon
    struct tm tm_input = {0};
    tm_input.tm_year = year - 1900;
    tm_input.tm_mon = month - 1;
    tm_input.tm_mday = day;
    tm_input.tm_hour = 12;
    time64_t dt = timegm(&tm_input);

    // Moon phase
    float phase = calculate_moon_phase(dt);

    // Sunrise/sunset
    int sr_h, sr_m, ss_h, ss_m;
    calculate_sunrise_sunset(dt, &sr_h, &sr_m, &ss_h, &ss_m);

    // Next full/new moon
    time64_t next_full, next_new;
    find_next_full_new_moon(dt, &next_full, &next_new);

    struct tm tm_full, tm_new;
    time_t t_full = (time_t)next_full;
    time_t t_new = (time_t)next_new;
    gmtime_r(&t_full, &tm_full);
    gmtime_r(&t_new, &tm_new);

    printf("Date: %04d-%02d-%02d\n", year, month, day);
    printf("Moon phase: %.3f (0=new/dark, 0.5=full/bright)\n", phase);
    printf("Sunrise: %02d:%02d (local)\n", sr_h, sr_m);
    printf("Sunset: %02d:%02d (local)\n", ss_h, ss_m);
    printf("Next full moon: %04d-%02d-%02d %02d:%02d UTC\n",
           tm_full.tm_year + 1900, tm_full.tm_mon + 1, tm_full.tm_mday,
           tm_full.tm_hour, tm_full.tm_min);
    printf("Next new moon: %04d-%02d-%02d %02d:%02d UTC\n",
           tm_new.tm_year + 1900, tm_new.tm_mon + 1, tm_new.tm_mday,
           tm_new.tm_hour, tm_new.tm_min);

    return 0;
}

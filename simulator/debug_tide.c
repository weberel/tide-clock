/**
 * Debug tool - uses EXACT firmware tide calculation code
 * to trace through what's happening on problematic dates.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef int64_t time64_t;

// ============================================================================
// EXACT COPY FROM FIRMWARE: src/tide.c
// ============================================================================

typedef struct {
    float amplitude;     // meters
    float omega;         // radians per hour
    float phase_rad;     // radians
} TideConstituent;

#define DEG_TO_RAD 0.01745329252f  // PI/180

// EXACT 31 constituents from firmware
static TideConstituent constituents[] = {
    {1.6236f, 28.9841042f * DEG_TO_RAD, 209.601f * DEG_TO_RAD},  // M2
    {0.4758f, 30.0000000f * DEG_TO_RAD, 30.617f * DEG_TO_RAD},   // S2
    {0.2846f, 28.4397295f * DEG_TO_RAD, 263.418f * DEG_TO_RAD},  // N2
    {0.1624f, 30.0821373f * DEG_TO_RAD, 196.250f * DEG_TO_RAD},  // K2
    {0.1031f, 15.0410686f * DEG_TO_RAD, 357.212f * DEG_TO_RAD},  // K1
    {0.1272f, 13.9430356f * DEG_TO_RAD, 59.267f * DEG_TO_RAD},   // O1
    {0.0361f, 14.9589314f * DEG_TO_RAD, 354.719f * DEG_TO_RAD},  // P1
    {0.0415f, 13.3986609f * DEG_TO_RAD, 79.892f * DEG_TO_RAD},   // Q1
    {0.0913f, 27.9682084f * DEG_TO_RAD, 187.004f * DEG_TO_RAD},  // MU2
    {0.0824f, 28.5125831f * DEG_TO_RAD, 330.693f * DEG_TO_RAD},  // NU2
    {0.0306f, 27.8953548f * DEG_TO_RAD, 313.497f * DEG_TO_RAD},  // 2N2
    {0.1087f, 29.5284789f * DEG_TO_RAD, 324.568f * DEG_TO_RAD},  // L2
    {0.0224f, 29.9589333f * DEG_TO_RAD, 15.603f * DEG_TO_RAD},   // T2
    {0.0301f, 31.0158958f * DEG_TO_RAD, 14.171f * DEG_TO_RAD},   // 2SM2
    {0.0475f, 29.4556253f * DEG_TO_RAD, 245.613f * DEG_TO_RAD},  // LAM2
    {0.0063f, 15.5854433f * DEG_TO_RAD, 22.363f * DEG_TO_RAD},   // J1
    {0.0049f, 16.1391017f * DEG_TO_RAD, 77.357f * DEG_TO_RAD},   // OO1
    {0.0065f, 12.8542862f * DEG_TO_RAD, 127.521f * DEG_TO_RAD},  // 2Q1
    {0.0083f, 13.4715145f * DEG_TO_RAD, 152.544f * DEG_TO_RAD},  // RHO1
    {0.0525f, 57.9682084f * DEG_TO_RAD, 38.983f * DEG_TO_RAD},   // M4
    {0.0225f, 58.9841042f * DEG_TO_RAD, 232.210f * DEG_TO_RAD},  // MS4
    {0.0184f, 57.4238337f * DEG_TO_RAD, 87.083f * DEG_TO_RAD},   // MN4
    {0.0063f, 59.0662415f * DEG_TO_RAD, 39.766f * DEG_TO_RAD},   // MK4
    {0.0009f, 60.0000000f * DEG_TO_RAD, 20.681f * DEG_TO_RAD},   // S4
    {0.0092f, 86.9523127f * DEG_TO_RAD, 346.092f * DEG_TO_RAD},  // M6
    {0.0048f, 86.4079380f * DEG_TO_RAD, 33.689f * DEG_TO_RAD},   // 2MN6
    {0.0070f, 87.9682084f * DEG_TO_RAD, 165.004f * DEG_TO_RAD},  // 2MS6
    {0.0198f, 44.0251729f * DEG_TO_RAD, 128.877f * DEG_TO_RAD},  // MK3
    {0.0266f, 42.9271398f * DEG_TO_RAD, 201.290f * DEG_TO_RAD},  // 2MK3
    {0.0087f, 1.0980331f * DEG_TO_RAD, 104.409f * DEG_TO_RAD},   // Mf
    {0.0075f, 0.5443747f * DEG_TO_RAD, 48.050f * DEG_TO_RAD},    // Mm
};
static const int num_constituents = 31;
static const float mean_sea_level = 2.64f;

#define MODEL_EPOCH_UNIX 1546301340L

// Empirical time correction - EXACT from firmware
#define NODAL_AMP      -4.399f
#define NODAL_PHASE    2015.613f
#define NODAL_OFFSET   0.454f
#define NODAL_PERIOD   18.61f
#define ANNUAL_AMP     1.689f
#define ANNUAL_PHASE  -0.266f
#define SEMIANN_AMP    0.887f
#define SEMIANN_PHASE -0.118f
#define TWO_PI         6.28318530718f
#define NODAL_OMEGA    (TWO_PI / NODAL_PERIOD)

float get_time_correction(int year, int month, int day) {
    float decimal_year = (float)year + (month - 1) / 12.0f + (day - 1) / 365.25f;
    float nodal = NODAL_AMP * sinf(NODAL_OMEGA * (decimal_year - NODAL_PHASE)) + NODAL_OFFSET;
    float annual = ANNUAL_AMP * sinf(TWO_PI * (decimal_year - ANNUAL_PHASE));
    float semiannual = SEMIANN_AMP * sinf(TWO_PI * 2.0f * (decimal_year - SEMIANN_PHASE));
    return nodal + annual + semiannual;
}

float calculate_tide_height(time64_t dt) {
    float hours = (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f;
    float height = mean_sea_level;

    for (int i = 0; i < num_constituents; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        height += constituents[i].amplitude * cosf(angle);
    }

    return height;
}

// Refine extremum - EXACT from firmware
static time64_t refine_extremum(time64_t coarse_time, int find_max) {
    time64_t left = coarse_time - 15 * 60;
    time64_t right = coarse_time + 15 * 60;

    const float phi = 0.618033988749895f;

    time64_t x1 = right - (time64_t)(phi * (right - left));
    time64_t x2 = left + (time64_t)(phi * (right - left));
    float f1 = calculate_tide_height(x1);
    float f2 = calculate_tide_height(x2);

    while ((right - left) > 30) {
        if (find_max ? (f1 > f2) : (f1 < f2)) {
            right = x2;
            x2 = x1;
            f2 = f1;
            x1 = right - (time64_t)(phi * (right - left));
            f1 = calculate_tide_height(x1);
        } else {
            left = x1;
            x1 = x2;
            f1 = f2;
            x2 = left + (time64_t)(phi * (right - left));
            f2 = calculate_tide_height(x2);
        }
    }

    return (left + right) / 2;
}

// Find next high/low - EXACT from firmware
typedef struct {
    time64_t time;
    float height;
    int valid;
} TideEvent;

void find_next_high_low(time64_t dt, TideEvent *next_high, TideEvent *next_low, int verbose) {
    time64_t coarse_high_time = 0, coarse_low_time = 0;
    next_high->valid = 0;
    next_low->valid = 0;

    float prev_prev = calculate_tide_height(dt);
    float prev = calculate_tide_height(dt + 30 * 60);

    if (verbose) {
        printf("Starting search from t=0:\n");
        printf("  t=0 min: h=%.3f m\n", prev_prev);
        printf("  t=30 min: h=%.3f m\n", prev);
    }

    for (int i = 2; i < 48; i++) {
        time64_t t = dt + (time64_t)i * 30 * 60;
        float curr = calculate_tide_height(t);

        if (verbose && i < 10) {
            printf("  t=%d min: h=%.3f m", i * 30, curr);
            if (prev > prev_prev && prev > curr) printf(" <-- LOCAL MAX");
            if (prev < prev_prev && prev < curr) printf(" <-- LOCAL MIN");
            printf("\n");
        }

        // Check for local maximum
        if (prev > prev_prev && prev > curr && !next_high->valid) {
            coarse_high_time = dt + (time64_t)(i - 1) * 30 * 60;
            next_high->valid = 1;
            if (verbose) printf("  Found coarse HW at t=%d min\n", (i-1) * 30);
        }
        // Check for local minimum
        if (prev < prev_prev && prev < curr && !next_low->valid) {
            coarse_low_time = dt + (time64_t)(i - 1) * 30 * 60;
            next_low->valid = 1;
            if (verbose) printf("  Found coarse LW at t=%d min\n", (i-1) * 30);
        }

        if (next_high->valid && next_low->valid) break;

        prev_prev = prev;
        prev = curr;
    }

    // Refine and apply correction
    if (next_high->valid) {
        next_high->time = refine_extremum(coarse_high_time, 1);
        next_high->height = calculate_tide_height(next_high->time);

        struct tm *tm_h = gmtime((time_t*)&next_high->time);
        float correction_min = get_time_correction(
            tm_h->tm_year + 1900, tm_h->tm_mon + 1, tm_h->tm_mday);
        next_high->time -= (time64_t)(correction_min * 60);

        if (verbose) {
            printf("  Refined HW: correction=%.1f min applied\n", correction_min);
        }
    }

    if (next_low->valid) {
        next_low->time = refine_extremum(coarse_low_time, 0);
        next_low->height = calculate_tide_height(next_low->time);

        struct tm *tm_l = gmtime((time_t*)&next_low->time);
        float correction_min = get_time_correction(
            tm_l->tm_year + 1900, tm_l->tm_mon + 1, tm_l->tm_mday);
        next_low->time -= (time64_t)(correction_min * 60);

        if (verbose) {
            printf("  Refined LW: correction=%.1f min applied\n", correction_min);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s YYYY-MM-DD_HH:MM [verbose]\n", argv[0]);
        printf("Example: %s 2025-01-29_12:00 verbose\n", argv[0]);
        return 1;
    }

    int year, month, day, hour, minute;
    if (sscanf(argv[1], "%d-%d-%d_%d:%d", &year, &month, &day, &hour, &minute) != 5) {
        printf("Invalid date format. Use YYYY-MM-DD_HH:MM\n");
        return 1;
    }

    int verbose = (argc > 2 && strcmp(argv[2], "verbose") == 0);

    // Create UTC timestamp
    struct tm tm_input = {0};
    tm_input.tm_year = year - 1900;
    tm_input.tm_mon = month - 1;
    tm_input.tm_mday = day;
    tm_input.tm_hour = hour;
    tm_input.tm_min = minute;
    time64_t dt = timegm(&tm_input);

    printf("=== Debug for %04d-%02d-%02d %02d:%02d UTC ===\n", year, month, day, hour, minute);
    printf("Unix timestamp: %ld\n", (long)dt);
    printf("Hours since model epoch: %.2f\n", (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f);
    printf("\n");

    // Show tide heights around this time
    printf("Tide heights around query time (15-min intervals, UTC):\n");
    for (int offset = -120; offset <= 240; offset += 15) {
        time64_t t = dt + offset * 60;
        float h = calculate_tide_height(t);
        struct tm *tm_t = gmtime((time_t*)&t);
        printf("  %02d:%02d UTC: %.3f m%s\n",
               tm_t->tm_hour, tm_t->tm_min, h,
               offset == 0 ? " <-- query time" : "");
    }
    printf("\n");

    // Run the algorithm
    printf("Running find_next_high_low algorithm:\n");
    TideEvent next_high, next_low;
    find_next_high_low(dt, &next_high, &next_low, verbose);

    printf("\n=== Results ===\n");
    if (next_high.valid) {
        struct tm *tm_h = gmtime((time_t*)&next_high.time);
        printf("Next HW: %02d:%02d UTC (height: %.2f m)\n",
               tm_h->tm_hour, tm_h->tm_min, next_high.height);
        printf("         Minutes from query: %ld\n", (long)(next_high.time - dt) / 60);
    }
    if (next_low.valid) {
        struct tm *tm_l = gmtime((time_t*)&next_low.time);
        printf("Next LW: %02d:%02d UTC (height: %.2f m)\n",
               tm_l->tm_hour, tm_l->tm_min, next_low.height);
        printf("         Minutes from query: %ld\n", (long)(next_low.time - dt) / 60);
    }

    return 0;
}

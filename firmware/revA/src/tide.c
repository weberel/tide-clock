/**
 * Tide Calculator for Margate, UK
 *
 * 31-constituent harmonic model fitted to PLA data (2019-2026).
 * Optimized for STM32F411 (Cortex-M4 with FPU).
 */

#include "tide.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Harmonic Constituents
// ============================================================================

typedef struct {
    float amplitude;     // meters
    float omega;         // radians per hour
    float phase_rad;     // radians
} TideConstituent;

#define DEG_TO_RAD 0.01745329252f  // PI/180

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

// Model epoch: 2019-01-01 00:09:00 UTC
#define MODEL_EPOCH_UNIX 1546301340L

// ============================================================================
// Empirical Time Correction
// ============================================================================

// Optimized from PLA HWLW timing data analysis (2019-2026)
// Corrects for 18.61-year lunar nodal cycle + annual + semi-annual effects
// Achieves MAE ~4.2 min timing accuracy
#define NODAL_AMP      -4.922f    // minutes
#define NODAL_PHASE    2015.522f  // year of zero crossing
#define NODAL_OFFSET   0.424f     // minutes
#define NODAL_PERIOD   18.61f     // years
#define ANNUAL_AMP     1.674f     // minutes
#define ANNUAL_PHASE  -0.263f     // year fraction
#define SEMIANN_AMP    0.699f     // minutes
#define SEMIANN_PHASE -0.129f     // year fraction
#define TWO_PI         6.28318530718f
#define NODAL_OMEGA    (TWO_PI / NODAL_PERIOD)

float get_time_correction(int year, int month, int day) {
    float decimal_year = (float)year + (month - 1) / 12.0f + (day - 1) / 365.25f;
    float nodal = NODAL_AMP * sinf(NODAL_OMEGA * (decimal_year - NODAL_PHASE)) + NODAL_OFFSET;
    float annual = ANNUAL_AMP * sinf(TWO_PI * (decimal_year - ANNUAL_PHASE));
    float semiannual = SEMIANN_AMP * sinf(TWO_PI * 2.0f * (decimal_year - SEMIANN_PHASE));
    return nodal + annual + semiannual;
}

// ============================================================================
// Tide Calculation with Analytical Derivatives
// ============================================================================

float calculate_tide_height(time64_t dt) {
    float hours = (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f;
    float height = mean_sea_level;

    for (int i = 0; i < num_constituents; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        height += constituents[i].amplitude * cosf(angle);
    }

    return height;
}

// First derivative: dh/dt in meters per hour
static float calculate_tide_derivative(time64_t dt) {
    float hours = (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f;
    float deriv = 0.0f;

    for (int i = 0; i < num_constituents; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        // d/dt[A*cos(ωt - φ)] = -A*ω*sin(ωt - φ)
        deriv -= constituents[i].amplitude * constituents[i].omega * sinf(angle);
    }

    return deriv;
}

// Second derivative: d²h/dt² in meters per hour²
static float calculate_tide_second_derivative(time64_t dt) {
    float hours = (float)(dt - MODEL_EPOCH_UNIX) / 3600.0f;
    float deriv2 = 0.0f;

    for (int i = 0; i < num_constituents; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        // d²/dt²[A*cos(ωt - φ)] = -A*ω²*cos(ωt - φ)
        deriv2 -= constituents[i].amplitude * constituents[i].omega * constituents[i].omega * cosf(angle);
    }

    return deriv2;
}

// Refine extremum using Newton-Raphson starting from coarse estimate
static time64_t refine_extremum_newton(time64_t coarse_time) {
    float t_hours = (float)(coarse_time - MODEL_EPOCH_UNIX) / 3600.0f;

    for (int iter = 0; iter < 20; iter++) {
        time64_t t_unix = MODEL_EPOCH_UNIX + (time64_t)(t_hours * 3600.0f);
        float h_prime = calculate_tide_derivative(t_unix);
        float h_double_prime = calculate_tide_second_derivative(t_unix);

        if (fabsf(h_double_prime) < 1e-10f) break;

        float delta = h_prime / h_double_prime;
        t_hours -= delta;

        if (fabsf(delta) < 1.0f / 3600.0f) break;
    }

    return MODEL_EPOCH_UNIX + (time64_t)(t_hours * 3600.0f);
}

void find_next_high_low(time64_t dt, TideEvent *next_high, TideEvent *next_low) {
    next_high->valid = 0;
    next_low->valid = 0;

    // Use derivative sign changes to find extrema (more robust than height comparison)
    // h'(t) changes from + to - at maxima, from - to + at minima
    float prev_deriv = calculate_tide_derivative(dt);
    time64_t coarse_high_time = 0, coarse_low_time = 0;

    // Search forward in 15-minute steps (finer than before)
    for (int i = 1; i < 100; i++) {  // Up to 25 hours
        time64_t t = dt + (time64_t)i * 15 * 60;
        float curr_deriv = calculate_tide_derivative(t);

        // Zero crossing from positive to negative = maximum
        if (prev_deriv > 0 && curr_deriv <= 0 && !next_high->valid) {
            coarse_high_time = t - 7 * 60;  // Midpoint of interval
            next_high->valid = 1;
        }
        // Zero crossing from negative to positive = minimum
        if (prev_deriv < 0 && curr_deriv >= 0 && !next_low->valid) {
            coarse_low_time = t - 7 * 60;
            next_low->valid = 1;
        }

        if (next_high->valid && next_low->valid) break;

        prev_deriv = curr_deriv;
    }

    // Refine using Newton-Raphson
    if (next_high->valid) {
        next_high->time = refine_extremum_newton(coarse_high_time);
        next_high->height = calculate_tide_height(next_high->time);
    }

    if (next_low->valid) {
        next_low->time = refine_extremum_newton(coarse_low_time);
        next_low->height = calculate_tide_height(next_low->time);
    }

    // Apply empirical time correction
    if (next_high->valid) {
        struct tm tm_h;
        time_to_tm(next_high->time, &tm_h);
        float correction_min = get_time_correction(
            tm_h.tm_year + 1900, tm_h.tm_mon + 1, tm_h.tm_mday);
        next_high->time -= (time64_t)(correction_min * 60);
    }

    if (next_low->valid) {
        struct tm tm_l;
        time_to_tm(next_low->time, &tm_l);
        float correction_min = get_time_correction(
            tm_l.tm_year + 1900, tm_l.tm_mon + 1, tm_l.tm_mday);
        next_low->time -= (time64_t)(correction_min * 60);
    }
}

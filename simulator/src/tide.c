/**
 * Tide Calculator
 *
 * 31-constituent harmonic model with empirical corrections.
 * Location-specific parameters loaded from location_config.h.
 * Optimized for STM32F411 (Cortex-M4 with FPU).
 */

#include "tide.h"
#include "location_config.h"
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

// Constituents array - initialized from location_config.h at startup
static TideConstituent constituents[TIDE_NUM_CONSTITUENTS];
static int tide_initialized = 0;

// Initialize tide model from location config
void tide_init(void) {
    if (tide_initialized) return;

    for (int i = 0; i < TIDE_NUM_CONSTITUENTS; i++) {
        constituents[i].amplitude = TIDE_CONSTITUENTS[i][0];
        constituents[i].omega = TIDE_CONSTITUENTS[i][1] * DEG_TO_RAD;
        constituents[i].phase_rad = TIDE_CONSTITUENTS[i][2] * DEG_TO_RAD;
    }
    tide_initialized = 1;
}

// ============================================================================
// Empirical Time Correction
// ============================================================================

// Parameters loaded from location_config.h
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
    if (!tide_initialized) tide_init();

    float hours = (float)(dt - TIDE_MODEL_EPOCH) / 3600.0f;
    float height = TIDE_MEAN_SEA_LEVEL;

    for (int i = 0; i < TIDE_NUM_CONSTITUENTS; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        height += constituents[i].amplitude * cosf(angle);
    }

    return height;
}

// First derivative: dh/dt in meters per hour
static float calculate_tide_derivative(time64_t dt) {
    float hours = (float)(dt - TIDE_MODEL_EPOCH) / 3600.0f;
    float deriv = 0.0f;

    for (int i = 0; i < TIDE_NUM_CONSTITUENTS; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        // d/dt[A*cos(ωt - φ)] = -A*ω*sin(ωt - φ)
        deriv -= constituents[i].amplitude * constituents[i].omega * sinf(angle);
    }

    return deriv;
}

// Second derivative: d²h/dt² in meters per hour²
static float calculate_tide_second_derivative(time64_t dt) {
    float hours = (float)(dt - TIDE_MODEL_EPOCH) / 3600.0f;
    float deriv2 = 0.0f;

    for (int i = 0; i < TIDE_NUM_CONSTITUENTS; i++) {
        float angle = constituents[i].omega * hours - constituents[i].phase_rad;
        // d²/dt²[A*cos(ωt - φ)] = -A*ω²*cos(ωt - φ)
        deriv2 -= constituents[i].amplitude * constituents[i].omega * constituents[i].omega * cosf(angle);
    }

    return deriv2;
}

// Refine extremum using Newton-Raphson starting from coarse estimate
static time64_t refine_extremum_newton(time64_t coarse_time) {
    float t_hours = (float)(coarse_time - TIDE_MODEL_EPOCH) / 3600.0f;

    for (int iter = 0; iter < 20; iter++) {
        time64_t t_unix = TIDE_MODEL_EPOCH + (time64_t)(t_hours * 3600.0f);
        float h_prime = calculate_tide_derivative(t_unix);
        float h_double_prime = calculate_tide_second_derivative(t_unix);

        if (fabsf(h_double_prime) < 1e-10f) break;

        float delta = h_prime / h_double_prime;
        t_hours -= delta;

        if (fabsf(delta) < 1.0f / 3600.0f) break;
    }

    return TIDE_MODEL_EPOCH + (time64_t)(t_hours * 3600.0f);
}

void find_next_high_low(time64_t dt, TideEvent *next_high, TideEvent *next_low) {
    if (!tide_initialized) tide_init();

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

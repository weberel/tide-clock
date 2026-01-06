#ifndef TIDE_H
#define TIDE_H

#include "timezone.h"

// Tide event structure
typedef struct {
    time64_t time;
    float height;
    int valid;
} TideEvent;

/**
 * Calculate tide height at given time.
 * Uses 31-constituent harmonic model fitted to PLA data (2019-2026).
 *
 * @param dt Unix timestamp (seconds since 1970-01-01 UTC)
 * @return Tide height in meters above Chart Datum
 */
float calculate_tide_height(time64_t dt);

/**
 * Find next high and low tide from given time.
 * Applies empirical time correction for nodal + seasonal effects.
 *
 * @param dt Current time (UTC)
 * @param next_high Output: next high water event
 * @param next_low Output: next low water event
 */
void find_next_high_low(time64_t dt, TideEvent *next_high, TideEvent *next_low);

/**
 * Calculate empirical time correction in minutes.
 * Accounts for 18.61-year nodal cycle + annual + semi-annual effects.
 *
 * @param year Calendar year (e.g., 2025)
 * @param month Month 1-12
 * @param day Day 1-31
 * @return Correction in minutes (subtract from predicted time)
 */
float get_time_correction(int year, int month, int day);

#endif // TIDE_H

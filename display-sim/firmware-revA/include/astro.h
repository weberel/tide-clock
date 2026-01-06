#ifndef ASTRO_H
#define ASTRO_H

#include "timezone.h"

/**
 * Calculate moon phase for display.
 * Uses Meeus algorithm (Astronomical Algorithms Ch.49).
 *
 * @param dt Unix timestamp (UTC)
 * @return Phase 0-1 where 0=new moon, 0.5=full moon
 */
float calculate_moon_phase(time64_t dt);

/**
 * Find next full moon and new moon after given time.
 * Accurate to ~3 minutes for dates 2000-2050+.
 *
 * @param dt Current time (UTC)
 * @param next_full Output: time of next full moon
 * @param next_new Output: time of next new moon
 */
void find_next_full_new_moon(time64_t dt, time64_t *next_full, time64_t *next_new);

/**
 * Calculate sunrise and sunset times for Margate.
 * Uses NOAA algorithm with atmospheric refraction correction.
 *
 * @param dt Date to calculate for (UTC)
 * @param sunrise_hour Output: sunrise hour (local time)
 * @param sunrise_min Output: sunrise minute
 * @param sunset_hour Output: sunset hour (local time)
 * @param sunset_min Output: sunset minute
 */
void calculate_sunrise_sunset(time64_t dt, int *sunrise_hour, int *sunrise_min,
                              int *sunset_hour, int *sunset_min);

#endif // ASTRO_H

#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <time.h>
#include <stdint.h>

// 64-bit time type - safe until year 292 billion
typedef int64_t time64_t;

/**
 * Convert 64-bit timestamp to struct tm (replaces gmtime).
 * Uses 64-bit math internally, safe beyond 2038.
 *
 * @param t Unix timestamp (UTC)
 * @param result Output struct tm
 */
void time_to_tm(time64_t t, struct tm *result);

/**
 * Check if a given UTC time is during British Summer Time (BST).
 * BST starts at 01:00 UTC on last Sunday of March.
 * BST ends at 01:00 UTC on last Sunday of October.
 *
 * @param utc_time Unix timestamp (UTC)
 * @return 1 if BST, 0 if GMT
 */
int is_bst(time64_t utc_time);

/**
 * Convert UTC time to Margate local time (struct tm).
 * Accounts for BST during summer months.
 *
 * @param utc_time Unix timestamp (UTC)
 * @return Pointer to static struct tm in local time
 */
struct tm* utc_to_margate(time64_t utc_time);

/**
 * Parse a date/time in Margate local time and convert to UTC.
 *
 * @param year Calendar year (e.g., 2025)
 * @param month Month 1-12
 * @param day Day 1-31
 * @param hour Hour 0-23
 * @param minute Minute 0-59
 * @return Unix timestamp (UTC)
 */
time64_t parse_margate_time(int year, int month, int day, int hour, int minute);

#endif // TIMEZONE_H

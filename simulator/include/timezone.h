#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <time.h>
#include <stdint.h>

// 64-bit time type - safe until year 292 billion
typedef int64_t time64_t;

// ============================================================================
// Timezone Rules
// ============================================================================
// Supported timezone rules for DST transitions.
// Selected via TZ_RULE in location_config.h

typedef enum {
    TZ_RULE_UTC,         // No DST
    TZ_RULE_UK,          // GMT/BST - Last Sun Mar/Oct at 01:00 UTC
    TZ_RULE_EU_CENTRAL,  // CET/CEST - Last Sun Mar/Oct at 01:00 UTC
    TZ_RULE_EU_EASTERN,  // EET/EEST - Last Sun Mar/Oct at 01:00 UTC
    TZ_RULE_US_EASTERN,  // EST/EDT - 2nd Sun Mar / 1st Sun Nov at 02:00 local
    TZ_RULE_US_CENTRAL,  // CST/CDT - 2nd Sun Mar / 1st Sun Nov at 02:00 local
    TZ_RULE_US_MOUNTAIN, // MST/MDT - 2nd Sun Mar / 1st Sun Nov at 02:00 local
    TZ_RULE_US_PACIFIC,  // PST/PDT - 2nd Sun Mar / 1st Sun Nov at 02:00 local
    TZ_RULE_US_ALASKA,   // AKST/AKDT - 2nd Sun Mar / 1st Sun Nov at 02:00 local
    TZ_RULE_US_HAWAII,   // HST - No DST
    TZ_RULE_AU_EASTERN,  // AEST/AEDT - 1st Sun Oct / 1st Sun Apr at 02:00 local
    TZ_RULE_AU_WESTERN,  // AWST - No DST
    TZ_RULE_NZ,          // NZST/NZDT - Last Sun Sep / 1st Sun Apr
} TzRule;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * Convert 64-bit timestamp to struct tm (replaces gmtime).
 * Uses 64-bit math internally, safe beyond 2038.
 *
 * @param t Unix timestamp (UTC)
 * @param result Output struct tm
 */
void time_to_tm(time64_t t, struct tm *result);

/**
 * Check if DST is in effect for configured timezone.
 *
 * @param utc_time Unix timestamp (UTC)
 * @return 1 if DST, 0 if standard time
 */
int is_dst(time64_t utc_time);

/**
 * Get UTC offset in seconds for configured timezone at given time.
 *
 * @param utc_time Unix timestamp (UTC)
 * @return Offset in seconds (positive = east of UTC)
 */
int get_utc_offset(time64_t utc_time);

/**
 * Convert UTC time to local time (struct tm).
 * Accounts for DST based on configured timezone.
 *
 * @param utc_time Unix timestamp (UTC)
 * @return Pointer to static struct tm in local time
 */
struct tm* utc_to_local(time64_t utc_time);

/**
 * Parse a date/time in local time and convert to UTC.
 *
 * @param year Calendar year (e.g., 2025)
 * @param month Month 1-12
 * @param day Day 1-31
 * @param hour Hour 0-23
 * @param minute Minute 0-59
 * @return Unix timestamp (UTC)
 */
time64_t parse_local_time(int year, int month, int day, int hour, int minute);

// ============================================================================
// Legacy Functions (for backward compatibility)
// ============================================================================

// These use the old Margate-specific names but call the generic functions
int is_bst(time64_t utc_time);
struct tm* utc_to_margate(time64_t utc_time);
time64_t parse_margate_time(int year, int month, int day, int hour, int minute);

#endif // TIMEZONE_H

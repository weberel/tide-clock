/**
 * ===========================================
 * LOCATION CONFIGURATION
 * ===========================================
 *
 * This file contains all location-specific parameters for the tide clock.
 * To customize for a different location, regenerate this file using:
 *
 *   python scripts/generate_location.py --help
 *
 * Location: Margate
 * Generated: 2026-01-07
 */

#ifndef LOCATION_CONFIG_H
#define LOCATION_CONFIG_H

// ============================================================================
// Location
// ============================================================================

#define LOCATION_NAME "Margate"  // Display name (keep short to fit screen)
#define LOCATION_LAT 51.3813f
#define LOCATION_LON 1.3862f

// ============================================================================
// Timezone
// ============================================================================
// Supported rules: TZ_RULE_UTC, TZ_RULE_UK, TZ_RULE_EU_CENTRAL, TZ_RULE_EU_EASTERN,
//                  TZ_RULE_US_EASTERN, TZ_RULE_US_CENTRAL, TZ_RULE_US_MOUNTAIN,
//                  TZ_RULE_US_PACIFIC, TZ_RULE_US_ALASKA, TZ_RULE_US_HAWAII,
//                  TZ_RULE_AU_EASTERN, TZ_RULE_AU_WESTERN, TZ_RULE_NZ

#define TZ_RULE TZ_RULE_UK
#define TZ_OFFSET_WINTER 0    // hours from UTC (standard time)
#define TZ_OFFSET_SUMMER 1    // hours from UTC (daylight saving time)

// ============================================================================
// Tidal Model
// ============================================================================

// Model epoch: 2019-01-01T00:09:00
#define TIDE_MODEL_EPOCH 1546297740L

// Mean sea level above chart datum (meters)
#define TIDE_MEAN_SEA_LEVEL 2.64f

// Number of harmonic constituents
#define TIDE_NUM_CONSTITUENTS 31

// Harmonic constituents: {amplitude (m), speed (deg/hr), phase (deg)}
// Constituents are stored in degrees and converted at init time
static const float TIDE_CONSTITUENTS[31][3] = {
    {1.6255f, 28.9841042f, 211.112f},  // M2 - Principal lunar semidiurnal
    {0.4758f, 30.0000000f,  30.505f},  // S2 - Principal solar semidiurnal
    {0.2849f, 28.4397295f, 264.939f},  // N2 - Larger lunar elliptic
    {0.1486f, 30.0821373f, 210.672f},  // K2 - Lunisolar semidiurnal
    {0.1007f, 15.0410686f,   3.883f},  // K1 - Lunisolar diurnal
    {0.1218f, 13.9430356f,  50.888f},  // O1 - Principal lunar diurnal
    {0.0361f, 14.9589314f, 354.989f},  // P1 - Principal solar diurnal
    {0.0398f, 13.3986609f,  71.628f},  // Q1 - Larger lunar elliptic diurnal
    {0.0914f, 27.9682084f, 188.528f},  // MU2 - Variational
    {0.0825f, 28.5125831f, 332.320f},  // NU2 - Larger lunar evectional
    {0.0307f, 27.8953548f, 315.021f},  // 2N2 - Lunar elliptic second order
    {0.1091f, 29.5284789f, 326.092f},  // L2 - Smaller lunar elliptic
    {0.0226f, 29.9589333f,  13.979f},  // T2 - Larger solar elliptic
    {0.0301f, 31.0158958f,  14.150f},  // 2SM2
    {0.0479f, 29.4556253f, 247.252f},  // LAM2 - Smaller lunar evectional
    {0.0062f, 15.5854433f,  29.079f},  // J1 - Smaller lunar elliptic diurnal
    {0.0044f, 16.1391017f,  92.913f},  // OO1 - Second order lunar diurnal
    {0.0062f, 12.8542862f, 119.265f},  // 2Q1
    {0.0078f, 13.4715145f, 143.576f},  // RHO1
    {0.0526f, 57.9682084f,  41.984f},  // M4 - Shallow water overtide
    {0.0225f, 58.9841042f, 233.610f},  // MS4 - Shallow water quarter diurnal
    {0.0183f, 57.4238337f,  90.070f},  // MN4
    {0.0059f, 59.0662415f,  53.979f},  // MK4
    {0.0009f, 60.0000000f,  20.457f},  // S4
    {0.0092f, 86.9523127f, 350.237f},  // M6 - Shallow water sixth diurnal
    {0.0048f, 86.4079380f,  37.674f},  // 2MN6
    {0.0070f, 87.9682084f, 167.597f},  // 2MS6
    {0.0196f, 44.0251729f, 136.838f},  // MK3 - Shallow water terdiurnal
    {0.0265f, 42.9271398f, 197.834f},  // 2MK3
    {0.0071f, 1.0980331f, 127.115f},  // Mf - Lunar fortnightly
    {0.0074f, 0.5443747f,  48.701f},  // Mm - Lunar monthly
};

// ============================================================================
// Empirical Time Corrections
// ============================================================================
// These correct for the 18.61-year lunar nodal cycle plus seasonal effects.
// Set to zero if not fitted for this location.

#define NODAL_AMP      0.000f    // minutes (amplitude of nodal correction)
#define NODAL_PHASE    2000.000f  // year of zero crossing
#define NODAL_OFFSET   0.000f     // minutes (DC offset)
#define NODAL_PERIOD   18.61f     // years (lunar nodal cycle)
#define ANNUAL_AMP     0.000f     // minutes (annual seasonal amplitude)
#define ANNUAL_PHASE  0.000f     // year fraction (annual phase)
#define SEMIANN_AMP    0.000f     // minutes (semi-annual amplitude)
#define SEMIANN_PHASE 0.000f     // year fraction (semi-annual phase)

#endif // LOCATION_CONFIG_H

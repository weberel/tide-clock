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
 * Location: Margate, UK
 * Generated: 2026-01-07
 */

#ifndef LOCATION_CONFIG_H
#define LOCATION_CONFIG_H

// ============================================================================
// Location
// ============================================================================

#define LOCATION_NAME "Margate"
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

// Model epoch: 2019-01-01 00:09:00 UTC
#define TIDE_MODEL_EPOCH 1546301340L

// Mean sea level above chart datum (meters)
#define TIDE_MEAN_SEA_LEVEL 2.64f

// Number of harmonic constituents
#define TIDE_NUM_CONSTITUENTS 31

// Harmonic constituents: {amplitude (m), speed (deg/hr), phase (deg)}
// Constituents are stored in degrees and converted at init time
static const float TIDE_CONSTITUENTS[31][3] = {
    {1.6236f, 28.9841042f, 209.601f},  // M2  - Principal lunar semidiurnal
    {0.4758f, 30.0000000f,  30.617f},  // S2  - Principal solar semidiurnal
    {0.2846f, 28.4397295f, 263.418f},  // N2  - Larger lunar elliptic
    {0.1624f, 30.0821373f, 196.250f},  // K2  - Lunisolar semidiurnal
    {0.1031f, 15.0410686f, 357.212f},  // K1  - Lunisolar diurnal
    {0.1272f, 13.9430356f,  59.267f},  // O1  - Principal lunar diurnal
    {0.0361f, 14.9589314f, 354.719f},  // P1  - Principal solar diurnal
    {0.0415f, 13.3986609f,  79.892f},  // Q1  - Larger lunar elliptic diurnal
    {0.0913f, 27.9682084f, 187.004f},  // MU2 - Variational
    {0.0824f, 28.5125831f, 330.693f},  // NU2 - Larger lunar evectional
    {0.0306f, 27.8953548f, 313.497f},  // 2N2 - Lunar elliptic second order
    {0.1087f, 29.5284789f, 324.568f},  // L2  - Smaller lunar elliptic
    {0.0224f, 29.9589333f,  15.603f},  // T2  - Larger solar elliptic
    {0.0301f, 31.0158958f,  14.171f},  // 2SM2
    {0.0475f, 29.4556253f, 245.613f},  // LAM2 - Smaller lunar evectional
    {0.0063f, 15.5854433f,  22.363f},  // J1  - Smaller lunar elliptic diurnal
    {0.0049f, 16.1391017f,  77.357f},  // OO1 - Second order lunar diurnal
    {0.0065f, 12.8542862f, 127.521f},  // 2Q1
    {0.0083f, 13.4715145f, 152.544f},  // RHO1
    {0.0525f, 57.9682084f,  38.983f},  // M4  - Shallow water overtide
    {0.0225f, 58.9841042f, 232.210f},  // MS4 - Shallow water quarter diurnal
    {0.0184f, 57.4238337f,  87.083f},  // MN4
    {0.0063f, 59.0662415f,  39.766f},  // MK4
    {0.0009f, 60.0000000f,  20.681f},  // S4
    {0.0092f, 86.9523127f, 346.092f},  // M6  - Shallow water sixth diurnal
    {0.0048f, 86.4079380f,  33.689f},  // 2MN6
    {0.0070f, 87.9682084f, 165.004f},  // 2MS6
    {0.0198f, 44.0251729f, 128.877f},  // MK3 - Shallow water terdiurnal
    {0.0266f, 42.9271398f, 201.290f},  // 2MK3
    {0.0087f,  1.0980331f, 104.409f},  // Mf  - Lunar fortnightly
    {0.0075f,  0.5443747f,  48.050f},  // Mm  - Lunar monthly
};

// ============================================================================
// Empirical Time Corrections
// ============================================================================
// These correct for the 18.61-year lunar nodal cycle plus seasonal effects.
// Fitted to Port of London Authority data (2019-2026).

#define NODAL_AMP      -4.922f    // minutes (amplitude of nodal correction)
#define NODAL_PHASE    2015.522f  // year of zero crossing
#define NODAL_OFFSET   0.424f     // minutes (DC offset)
#define NODAL_PERIOD   18.61f     // years (lunar nodal cycle)
#define ANNUAL_AMP     1.674f     // minutes (annual seasonal amplitude)
#define ANNUAL_PHASE  -0.263f     // year fraction (annual phase)
#define SEMIANN_AMP    0.699f     // minutes (semi-annual amplitude)
#define SEMIANN_PHASE -0.129f     // year fraction (semi-annual phase)

#endif // LOCATION_CONFIG_H

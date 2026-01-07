#ifndef POWER_H
#define POWER_H

#include <time.h>

// Wakeup reasons
typedef enum {
    WAKEUP_UNKNOWN = 0,
    WAKEUP_RTC_ALARM,       // DS3231 alarm triggered
    WAKEUP_NFC_FIELD,       // NFC field detected (phone tap)
    WAKEUP_BUTTON,          // Button press (if implemented)
    WAKEUP_RESET            // Power-on or reset
} WakeupReason;

/**
 * Initialize power management.
 * Configure wakeup sources and low-power settings.
 */
void power_init(void);

/**
 * Calculate next wakeup time based on tide schedule.
 * Wakes up before each high/low tide to update display.
 *
 * @param now Current time (UTC)
 * @return Time for next wakeup (UTC)
 */
time_t power_calculate_next_wakeup(time_t now);

/**
 * Enter shutdown mode.
 * Only wakes on PA0 rising edge (RTC alarm or NFC field).
 * Does not return - execution restarts from reset.
 */
void power_enter_shutdown(void);

/**
 * Enter deep sleep mode.
 * Wakes on RTC, NFC, or configured timeout.
 * Returns after waking.
 */
void power_enter_deep_sleep(void);

/**
 * Get reason for last wakeup.
 * @return Wakeup source that triggered this boot
 */
WakeupReason power_get_wakeup_reason(void);

/**
 * Check if we woke from NFC (phone tap).
 * If so, need to read time from NFC chip.
 *
 * @return 1 if NFC triggered wakeup, 0 otherwise
 */
int power_woke_from_nfc(void);

/**
 * Get time spent in last sleep (if trackable).
 * @return Seconds spent sleeping, or 0 if unknown
 */
uint32_t power_get_sleep_duration(void);

// ============================================================================
// Platform-specific functions (implement in power_stm32.c)
// ============================================================================

/**
 * Platform-specific shutdown implementation.
 */
void power_platform_shutdown(void);

/**
 * Platform-specific deep sleep implementation.
 */
void power_platform_deep_sleep(void);

/**
 * Configure PA0 for wakeup on rising edge.
 */
void power_platform_configure_wakeup(void);

#endif // POWER_H

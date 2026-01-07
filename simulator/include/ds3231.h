#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include <time.h>

// DS3231MZ I2C address
#define DS3231_ADDR 0x68

// DS3231 Register addresses
#define DS3231_REG_SECONDS  0x00
#define DS3231_REG_MINUTES  0x01
#define DS3231_REG_HOURS    0x02
#define DS3231_REG_DAY      0x03
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_A1_SEC   0x07
#define DS3231_REG_A1_MIN   0x08
#define DS3231_REG_A1_HOUR  0x09
#define DS3231_REG_A1_DAY   0x0A
#define DS3231_REG_A2_MIN   0x0B
#define DS3231_REG_A2_HOUR  0x0C
#define DS3231_REG_A2_DAY   0x0D
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F
#define DS3231_REG_TEMP_MSB 0x11
#define DS3231_REG_TEMP_LSB 0x12

// Control register bits
#define DS3231_CTRL_A1IE    0x01  // Alarm 1 Interrupt Enable
#define DS3231_CTRL_A2IE    0x02  // Alarm 2 Interrupt Enable
#define DS3231_CTRL_INTCN   0x04  // Interrupt Control (1=alarm, 0=square wave)
#define DS3231_CTRL_CONV    0x20  // Convert Temperature

// Status register bits
#define DS3231_STAT_A1F     0x01  // Alarm 1 Flag
#define DS3231_STAT_A2F     0x02  // Alarm 2 Flag
#define DS3231_STAT_BSY     0x04  // Busy (temperature conversion)

// Alarm mask bits (A1M1-A1M4, A2M2-A2M4)
#define DS3231_ALARM_MATCH_SEC    0x00  // Match seconds
#define DS3231_ALARM_MATCH_MIN    0x08  // Match minutes (ignore seconds)
#define DS3231_ALARM_MATCH_HOUR   0x0C  // Match hours (ignore min, sec)
#define DS3231_ALARM_MATCH_DATE   0x00  // Match date + hour + min + sec
#define DS3231_ALARM_MATCH_DAY    0x40  // Match day of week instead of date

/**
 * Initialize DS3231 RTC.
 * Configures for alarm interrupt output on INT/SQW pin.
 */
void ds3231_init(void);

/**
 * Read current time from DS3231.
 * @return Unix timestamp (UTC)
 */
time_t ds3231_get_time(void);

/**
 * Set DS3231 time.
 * @param t Unix timestamp (UTC) to set
 */
void ds3231_set_time(time_t t);

/**
 * Set Alarm 1 to trigger at specific time.
 * INT pin will go LOW when alarm triggers.
 *
 * @param wake_time Unix timestamp (UTC) for alarm
 */
void ds3231_set_alarm(time_t wake_time);

/**
 * Clear alarm flags and re-arm for next alarm.
 * Call this after waking from alarm.
 */
void ds3231_clear_alarm(void);

/**
 * Check if alarm has triggered.
 * @return 1 if alarm flag set, 0 otherwise
 */
int ds3231_alarm_triggered(void);

/**
 * Read DS3231 internal temperature sensor.
 * Resolution: 0.25C, range: -40C to +85C
 *
 * @return Temperature in degrees Celsius
 */
float ds3231_get_temperature(void);

/**
 * Disable all alarms and outputs.
 * Use before entering low-power mode if not using alarms.
 */
void ds3231_disable_alarms(void);

// ============================================================================
// Platform-specific I2C functions (implement in ds3231_platform.c)
// ============================================================================

/**
 * Write bytes to DS3231 register.
 * @param reg Starting register address
 * @param data Data buffer to write
 * @param len Number of bytes to write
 */
void ds3231_i2c_write(uint8_t reg, const uint8_t *data, uint8_t len);

/**
 * Read bytes from DS3231 register.
 * @param reg Starting register address
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 */
void ds3231_i2c_read(uint8_t reg, uint8_t *data, uint8_t len);

#endif // DS3231_H

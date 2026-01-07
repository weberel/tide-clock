/**
 * ST25DV04K NFC Tag Driver
 *
 * Dynamic NFC/RFID tag IC with 4Kbit EEPROM
 * I2C address: 0x53 (user memory), 0x57 (system memory)
 *
 * Hardware connections (Rev B):
 * - SDA/SCL: Shared I2C bus with DS3231 (PB6/PB7)
 * - LPD: Low Power Down pin (PA2) - HIGH = low power mode, LOW = active
 * - GPO: General Purpose Output (directly to PA0 via OR diode) - CMOS output
 *
 * GPO is configured for RF field detection (rising edge when field present)
 * OR'd with DS3231 alarm signal to PA0 for wake-up
 */

#ifndef ST25DV_H
#define ST25DV_H

#include <stdint.h>
#include <stdbool.h>

// I2C addresses (7-bit)
#define ST25DV_ADDR_USER    0x53    // User EEPROM area
#define ST25DV_ADDR_SYSTEM  0x57    // System configuration area

// GPO configuration bits (GPO register)
#define ST25DV_GPO_ENABLE       (1 << 7)    // GPO output enable
#define ST25DV_GPO_FIELD_CHANGE (1 << 0)    // Interrupt on RF field change

// Dynamic registers (accessible without password)
#define ST25DV_REG_GPO_CTRL_DYN     0x2000  // GPO control (dynamic)
#define ST25DV_REG_EH_CTRL_DYN      0x2002  // Energy harvesting control
#define ST25DV_REG_RF_MNGT_DYN      0x2003  // RF management
#define ST25DV_REG_IT_STS_DYN       0x2005  // Interrupt status
#define ST25DV_REG_MB_CTRL_DYN      0x2006  // Mailbox control

// Static registers (in system area, need password for write)
#define ST25DV_REG_GPO              0x0000  // GPO configuration
#define ST25DV_REG_IT_TIME          0x0001  // Interrupt pulse duration
#define ST25DV_REG_RF_MNGT          0x0003  // RF management config
#define ST25DV_REG_I2C_PWD          0x0900  // I2C password (64-bit)

// Mailbox registers
#define ST25DV_REG_MB_LEN_DYN       0x2007  // Mailbox message length
#define ST25DV_MAILBOX_RAM          0x2008  // Mailbox RAM start (256 bytes max)

// User EEPROM
#define ST25DV_USER_MEMORY          0x0000  // User memory start
#define ST25DV_USER_MEMORY_SIZE     512     // 4Kbit = 512 bytes

// Time sync protocol (written by phone to mailbox or user memory)
// Same format as NT3H2111 for compatibility
#define TIME_SYNC_BLOCK     0x0010  // User memory offset for time data
#define TIME_SYNC_MAGIC     0xA5    // Valid time marker

typedef struct {
    uint8_t year;       // Year - 2000
    uint8_t month;      // 1-12
    uint8_t day;        // 1-31
    uint8_t hour;       // 0-23
    uint8_t minute;     // 0-59
    uint8_t second;     // 0-59
    uint8_t checksum;   // XOR of bytes 0-5
    uint8_t magic;      // TIME_SYNC_MAGIC if valid
} TimeSyncData;

/**
 * Initialize ST25DV04K
 * - Exit low power mode (LPD pin LOW)
 * - Configure GPO for field detection
 *
 * @return true if device responds on I2C
 */
bool st25dv_init(void);

/**
 * Enter low power mode (LPD pin HIGH)
 * GPO still functions for field detection wake-up
 */
void st25dv_enter_low_power(void);

/**
 * Exit low power mode (LPD pin LOW)
 */
void st25dv_exit_low_power(void);

/**
 * Check if RF field is present
 * Reads IT_STS_DYN register
 *
 * @return true if RF field detected
 */
bool st25dv_is_field_present(void);

/**
 * Clear interrupt status flags
 * Call after handling GPO interrupt
 */
void st25dv_clear_interrupt(void);

/**
 * Read user EEPROM
 *
 * @param addr Address in user memory (0x0000 - 0x01FF)
 * @param data Buffer to read into
 * @param len Number of bytes to read
 * @return true if read successful
 */
bool st25dv_read_user(uint16_t addr, uint8_t *data, uint16_t len);

/**
 * Write user EEPROM
 * Note: EEPROM has limited write cycles (~1M)
 *
 * @param addr Address in user memory
 * @param data Data to write
 * @param len Number of bytes to write
 * @return true if write successful
 */
bool st25dv_write_user(uint16_t addr, const uint8_t *data, uint16_t len);

/**
 * Check for time sync data written by phone
 *
 * @param data Pointer to TimeSyncData struct to fill
 * @return true if valid time data found
 */
bool st25dv_read_time_sync(TimeSyncData *data);

/**
 * Clear time sync data after reading
 * Invalidates the magic byte so we don't read same data twice
 */
void st25dv_clear_time_sync(void);

// TODO: Implement mailbox functions for fast transfer mode
// bool st25dv_mailbox_enable(void);
// bool st25dv_mailbox_read(uint8_t *data, uint8_t *len);
// bool st25dv_mailbox_write(const uint8_t *data, uint8_t len);

// TODO: Implement NDEF message parsing
// bool st25dv_read_ndef_text(char *text, uint16_t max_len);
// bool st25dv_write_ndef_url(const char *url);

#endif // ST25DV_H

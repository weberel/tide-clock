#ifndef NT3H2111_H
#define NT3H2111_H

#include <stdint.h>
#include <time.h>

// NT3H2111 I2C address (directly connected, no address pins used)
#define NT3H2111_ADDR 0x55

// Memory layout
#define NT3H2111_BLOCK_SIZE     16      // Bytes per I2C block
#define NT3H2111_USER_MEM_START 0x01    // First user memory block
#define NT3H2111_USER_MEM_END   0x38    // Last user memory block (888 bytes total)
#define NT3H2111_SESSION_REG    0xFE    // Session registers
#define NT3H2111_CONFIG_REG     0x3A    // Configuration registers

// Session register offsets (within block 0xFE)
#define NT3H2111_NC_REG         0x00    // NC_REG - configuration
#define NT3H2111_LAST_NDEF      0x01    // Last NDEF block
#define NT3H2111_SRAM_MIRROR    0x02    // SRAM mirror block
#define NT3H2111_WDT_LS         0x03    // Watchdog timer LS
#define NT3H2111_WDT_MS         0x04    // Watchdog timer MS
#define NT3H2111_I2C_CLOCK_STR  0x05    // I2C clock stretching
#define NT3H2111_NS_REG         0x06    // NS_REG - status

// NC_REG bits
#define NT3H2111_NC_FD_OFF      0x00    // Field detect output disabled
#define NT3H2111_NC_FD_ON       0x01    // FD pin active during RF field
#define NT3H2111_NC_PTHRU_ON    0x40    // SRAM pass-through mode
#define NT3H2111_NC_SRAM_MIRROR 0x02    // SRAM mirror mode

// NS_REG bits (read-only status)
#define NT3H2111_NS_RF_FIELD    0x01    // RF field present
#define NT3H2111_NS_NDEF_READ   0x10    // NDEF data was read by NFC
#define NT3H2111_NS_SRAM_RF_RDY 0x20    // SRAM RF ready
#define NT3H2111_NS_SRAM_I2C_RDY 0x40   // SRAM I2C ready
#define NT3H2111_NS_EEPROM_WR   0x80    // EEPROM write in progress

// ============================================================================
// Time Sync Protocol
// ============================================================================
// Phone writes current time to a fixed memory location.
// Format: 6 bytes at user memory block 0x01
//   Byte 0: Year - 2000 (e.g., 25 for 2025)
//   Byte 1: Month (1-12)
//   Byte 2: Day (1-31)
//   Byte 3: Hour (0-23)
//   Byte 4: Minute (0-59)
//   Byte 5: Second (0-59)
//   Byte 6: Checksum (XOR of bytes 0-5)
//   Byte 7: Magic byte (0xA5 = valid data written)

#define NT3H2111_TIME_BLOCK     0x01    // Block containing time data
#define NT3H2111_TIME_MAGIC     0xA5    // Magic byte indicating valid time

typedef struct {
    uint8_t year;       // Year - 2000
    uint8_t month;      // 1-12
    uint8_t day;        // 1-31
    uint8_t hour;       // 0-23
    uint8_t minute;     // 0-59
    uint8_t second;     // 0-59
    uint8_t checksum;   // XOR of above
    uint8_t magic;      // 0xA5 if valid
} NfcTimeData;

/**
 * Initialize NT3H2111.
 * Enables field detect output on FD pin.
 */
void nt3h2111_init(void);

/**
 * Check if NFC field is present.
 * @return 1 if RF field detected, 0 otherwise
 */
int nt3h2111_field_present(void);

/**
 * Check if phone has written new time data.
 * @return 1 if valid time data available, 0 otherwise
 */
int nt3h2111_time_available(void);

/**
 * Read time data written by phone.
 * Clears the magic byte after reading.
 *
 * @return Unix timestamp (UTC) or 0 if invalid
 */
time_t nt3h2111_read_time(void);

/**
 * Write device status for phone to read.
 * Phone can read this via NFC to see device state.
 *
 * @param battery_mv Battery voltage in millivolts
 * @param last_update Unix timestamp of last display update
 */
void nt3h2111_write_status(uint16_t battery_mv, time_t last_update);

/**
 * Read raw memory block.
 * @param block Block number (0x01-0x38 for user memory)
 * @param data Buffer to store 16 bytes
 */
void nt3h2111_read_block(uint8_t block, uint8_t *data);

/**
 * Write raw memory block.
 * @param block Block number (0x01-0x38 for user memory)
 * @param data 16 bytes to write
 */
void nt3h2111_write_block(uint8_t block, const uint8_t *data);

/**
 * Read session register.
 * @param reg Register offset within session block
 * @return Register value
 */
uint8_t nt3h2111_read_session_reg(uint8_t reg);

/**
 * Write session register.
 * Uses mask/data format required by NT3H2111.
 *
 * @param reg Register offset
 * @param mask Bits to modify
 * @param data New values for masked bits
 */
void nt3h2111_write_session_reg(uint8_t reg, uint8_t mask, uint8_t data);

// ============================================================================
// Platform-specific I2C functions (implement in nt3h2111_platform.c)
// ============================================================================

/**
 * Write bytes to NT3H2111.
 * @param addr Memory address (block number)
 * @param data Data buffer to write
 * @param len Number of bytes to write (max 16)
 */
void nt3h2111_i2c_write(uint8_t addr, const uint8_t *data, uint8_t len);

/**
 * Read bytes from NT3H2111.
 * @param addr Memory address (block number)
 * @param data Buffer to store read data
 * @param len Number of bytes to read (max 16)
 */
void nt3h2111_i2c_read(uint8_t addr, uint8_t *data, uint8_t len);

#endif // NT3H2111_H

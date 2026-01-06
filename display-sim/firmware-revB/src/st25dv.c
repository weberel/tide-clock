/**
 * ST25DV04K NFC Tag Driver Implementation
 *
 * Rev B hardware:
 * - LPD on PA2 (LOW = active, HIGH = low power)
 * - GPO directly to PA0 via OR diode (CMOS output, HIGH when field detected)
 * - Shared I2C bus with DS3231 on PB6/PB7
 */

#include "st25dv.h"
#include "stm32f4xx_hal.h"

// Pin definitions (Rev B)
#define NFC_LPD_PIN     GPIO_PIN_2
#define NFC_LPD_PORT    GPIOA

// External I2C handle (shared with DS3231, defined in main.c)
extern I2C_HandleTypeDef hi2c;

// I2C timeout
#define I2C_TIMEOUT     100

/**
 * Read from ST25DV register
 */
static bool st25dv_read_reg(uint8_t i2c_addr, uint16_t reg_addr, uint8_t *data, uint16_t len) {
    uint8_t addr_buf[2] = {(reg_addr >> 8) & 0xFF, reg_addr & 0xFF};

    if (HAL_I2C_Master_Transmit(&hi2c, i2c_addr << 1, addr_buf, 2, I2C_TIMEOUT) != HAL_OK) {
        return false;
    }

    if (HAL_I2C_Master_Receive(&hi2c, i2c_addr << 1, data, len, I2C_TIMEOUT) != HAL_OK) {
        return false;
    }

    return true;
}

/**
 * Write to ST25DV register
 */
static bool st25dv_write_reg(uint8_t i2c_addr, uint16_t reg_addr, const uint8_t *data, uint16_t len) {
    // For EEPROM writes, need to send address + data in single transaction
    uint8_t buf[2 + 256];  // Max write size
    if (len > 256) return false;

    buf[0] = (reg_addr >> 8) & 0xFF;
    buf[1] = reg_addr & 0xFF;
    memcpy(&buf[2], data, len);

    if (HAL_I2C_Master_Transmit(&hi2c, i2c_addr << 1, buf, 2 + len, I2C_TIMEOUT) != HAL_OK) {
        return false;
    }

    // EEPROM write cycle time ~5ms
    HAL_Delay(5);

    return true;
}

bool st25dv_init(void) {
    // Configure LPD pin as output
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = NFC_LPD_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(NFC_LPD_PORT, &GPIO_InitStruct);

    // Exit low power mode
    st25dv_exit_low_power();

    // Small delay for device to wake
    HAL_Delay(1);

    // Check if device responds
    uint8_t dummy;
    if (!st25dv_read_reg(ST25DV_ADDR_USER, 0x0000, &dummy, 1)) {
        return false;
    }

    // Configure GPO for field detection
    // Note: Writing to system registers requires I2C password presentation
    // For now, assume factory default GPO config (field change detection enabled)

    // TODO: Present I2C password and configure GPO register if needed
    // Default factory config should have GPO enabled for field detection

    return true;
}

void st25dv_enter_low_power(void) {
    HAL_GPIO_WritePin(NFC_LPD_PORT, NFC_LPD_PIN, GPIO_PIN_SET);
}

void st25dv_exit_low_power(void) {
    HAL_GPIO_WritePin(NFC_LPD_PORT, NFC_LPD_PIN, GPIO_PIN_RESET);
}

bool st25dv_is_field_present(void) {
    uint8_t it_sts;
    if (!st25dv_read_reg(ST25DV_ADDR_USER, ST25DV_REG_IT_STS_DYN, &it_sts, 1)) {
        return false;
    }
    // Bit 0 = RF_USER flag (RF field detected)
    return (it_sts & 0x01) != 0;
}

void st25dv_clear_interrupt(void) {
    // Reading IT_STS_DYN clears the interrupt flags
    uint8_t dummy;
    st25dv_read_reg(ST25DV_ADDR_USER, ST25DV_REG_IT_STS_DYN, &dummy, 1);
}

bool st25dv_read_user(uint16_t addr, uint8_t *data, uint16_t len) {
    if (addr + len > ST25DV_USER_MEMORY_SIZE) {
        return false;
    }
    return st25dv_read_reg(ST25DV_ADDR_USER, addr, data, len);
}

bool st25dv_write_user(uint16_t addr, const uint8_t *data, uint16_t len) {
    if (addr + len > ST25DV_USER_MEMORY_SIZE) {
        return false;
    }
    return st25dv_write_reg(ST25DV_ADDR_USER, addr, data, len);
}

bool st25dv_read_time_sync(TimeSyncData *data) {
    if (!st25dv_read_user(TIME_SYNC_BLOCK, (uint8_t *)data, sizeof(TimeSyncData))) {
        return false;
    }

    // Check magic byte
    if (data->magic != TIME_SYNC_MAGIC) {
        return false;
    }

    // Verify checksum
    uint8_t calc_checksum = data->year ^ data->month ^ data->day ^
                            data->hour ^ data->minute ^ data->second;
    if (calc_checksum != data->checksum) {
        return false;
    }

    // Basic sanity checks
    if (data->month < 1 || data->month > 12 ||
        data->day < 1 || data->day > 31 ||
        data->hour > 23 || data->minute > 59 || data->second > 59) {
        return false;
    }

    return true;
}

void st25dv_clear_time_sync(void) {
    // Write 0x00 to magic byte to invalidate
    uint8_t zero = 0x00;
    st25dv_write_user(TIME_SYNC_BLOCK + 7, &zero, 1);
}

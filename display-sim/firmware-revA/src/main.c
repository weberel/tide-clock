/**
 * Tide Display - STM32 Main Entry Point
 *
 * Wake cycle (currently every 2 minutes for power testing):
 * 1. Wake from STOP mode (triggered by DS3231 alarm on PA0)
 * 2. Clear DS3231 alarm flags
 * 3. Read time from RTC
 * 4. Update e-paper display (non-blocking)
 * 5. Set next alarm
 * 6. Enter STOP mode
 */

#include "stm32f4xx_hal.h"
#include "display.h"
#include "render.h"
#include "tide.h"
#include "astro.h"
#include "timezone.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

// Debug flags - set to 1 to enable
#define DEBUG_MESSAGES 0        // Show all special messages on boot
#define DEBUG_FAST_REFRESH 0    // Continuous fast refresh every 15s to test ghosting
#define DEBUG_FRAME_TEST 0      // Show 10 test frames (10s each) for verification

// Pin definitions
#define LED1_PIN    GPIO_PIN_2
#define LED1_PORT   GPIOD
#define LED2_PIN    GPIO_PIN_11
#define LED2_PORT   GPIOC

#define BTN_PIN     GPIO_PIN_1   // PB1 - Button (active low with pull-up)
#define BTN_PORT    GPIOB

#define RTC_INT_PIN GPIO_PIN_0   // PA0 - DS3231 INT (active low)
#define RTC_PWR     GPIO_PIN_1   // PA1 - DS3231 power gate
#define NFC_PWR     GPIO_PIN_2   // PA2 - NT3H2111 power gate
#define EPD_PWR     GPIO_PIN_3   // PA3 - Display power gate

#define VBAT_PIN    GPIO_PIN_0   // PB0 - Battery voltage (ADC1_IN8)
#define VBAT_PORT   GPIOB

// Battery thresholds (2x AA in series)
// ADC reads 0-4095 for 0-3.3V (assuming direct connection, no divider)
// Fresh: ~3.2V, Empty: ~2.0V
#define VBAT_FULL_MV     3200    // Fresh batteries (2x 1.6V)
#define VBAT_EMPTY_MV    2000    // Dead batteries (2x 1.0V)
#define VBAT_CRITICAL_MV 2200    // Show "change batteries" warning

// DS3231 registers
#define DS3231_ADDR         0x68
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F
#define DS3231_REG_ALARM1   0x07  // Alarm 1 seconds register

// I2C for DS3231
static I2C_HandleTypeDef hi2c;

// ADC for battery voltage
static ADC_HandleTypeDef hadc;

// Forward declarations
void SystemClock_Config(void);
void GPIO_Init(void);
void I2C_Init(void);
time64_t ds3231_read_time(void);
void ds3231_set_time(int year, int month, int day, int hour, int min, int sec);
int ds3231_check_and_clear_alarm(void);
void ds3231_set_alarm_minutes(int minutes_ahead);
void enter_standby_mode(void);
void enter_stop_mode_wait_display(void);

// Special date messages lookup table
typedef struct {
    int month;
    int day;
    const char *message;
} DateMessage;

static const DateMessage special_dates[] = {
    // Holidays
    {1, 1, "Happy New Year!"},
    {2, 14, "Happy Valentine's!"},
    {10, 31, "Happy Halloween!"},
    {12, 24, "Merry Christmas!"},
    {12, 31, "Happy New Year!"},
    // Seasons
    {3, 1, "Spring is here!"},
    {6, 1, "Hello Summer"},
    {9, 1, "Autumn begins"},
    {12, 1, "Winter is here"},
    // Fun
    {3, 14, "3.1415926535897.."},
    // Meteor showers (peak nights)
    {1, 3, "Quadrantids tonight"},
    {1, 4, "Quadrantids tonight"},
    {4, 22, "Lyrids tonight"},
    {8, 11, "Perseids tonight"},
    {8, 12, "Perseids tonight"},
    {8, 13, "Perseids tonight"},
    {10, 21, "Orionids tonight"},
    {11, 17, "Leonids tonight"},
    {12, 13, "Geminids tonight"},
    {12, 14, "Geminids tonight"},
    {12, 15, "Geminids tonight"},
    {0, 0, NULL}  // End marker
};

// May 1st rotating messages - cycles each year
static const char* may_day_messages[] = {
    "Power to the people",
    "Solidarity forever",
    "Workers unite",
    "Seize the means",
    "Organize, agitate",
    "Unite and fight",
    "International unity",
    NULL  // End marker
};

// Eclipse dates visible from UK/Margate (2025-2125)
// Stored as packed value: year * 10000 + month * 100 + day
typedef struct {
    int date;  // YYYYMMDD format
    const char *message;
} EclipseDate;

static const EclipseDate eclipse_dates[] = {
    // 2020s
    {20251207, "Lunar Eclipse"},
    {20260812, "Solar Eclipse"},
    {20281231, "Lunar Eclipse"},
    {20290626, "Lunar Eclipse"},
    {20291220, "Lunar Eclipse"},
    // 2030s
    {20321018, "Lunar Eclipse"},
    {20330414, "Lunar Eclipse"},
    // 2040s
    {20430325, "Lunar Eclipse"},
    {20430919, "Lunar Eclipse"},
    {20440313, "Lunar Eclipse"},
    {20440907, "Lunar Eclipse"},
    {20470112, "Lunar Eclipse"},
    {20470707, "Lunar Eclipse"},
    {20480101, "Lunar Eclipse"},
    // 2050s
    {20500506, "Lunar Eclipse"},
    {20500514, "Solar Eclipse"},
    {20501030, "Lunar Eclipse"},
    {20510426, "Lunar Eclipse"},
    {20511019, "Lunar Eclipse"},
    {20540222, "Lunar Eclipse"},
    {20540818, "Lunar Eclipse"},
    {20550211, "Lunar Eclipse"},
    {20580606, "Lunar Eclipse"},
    {20581130, "Lunar Eclipse"},
    {20591105, "Solar Eclipse"},
    // 2060s
    {20610404, "Lunar Eclipse"},
    {20610929, "Lunar Eclipse"},
    {20620325, "Lunar Eclipse"},
    {20620918, "Lunar Eclipse"},
    {20650122, "Lunar Eclipse"},
    {20650205, "Solar Eclipse"},
    {20650717, "Lunar Eclipse"},
    {20660111, "Lunar Eclipse"},
    {20681109, "Lunar Eclipse"},
    {20690506, "Lunar Eclipse"},
    {20691030, "Lunar Eclipse"},
    // 2070s
    {20720304, "Lunar Eclipse"},
    {20720828, "Lunar Eclipse"},
    {20720912, "Solar Eclipse"},
    {20730222, "Lunar Eclipse"},
    {20730817, "Lunar Eclipse"},
    {20750713, "Solar Eclipse"},
    {20760617, "Lunar Eclipse"},
    {20761210, "Lunar Eclipse"},
    {20780511, "Solar Eclipse"},
    {20790501, "Solar Eclipse"},
    {20791010, "Lunar Eclipse"},
    // 2080s
    {20800404, "Lunar Eclipse"},
    {20800913, "Solar Eclipse"},
    {20800929, "Lunar Eclipse"},
    {20810903, "Solar Eclipse"},
    {20820227, "Solar Eclipse"},
    {20830203, "Lunar Eclipse"},
    {20830729, "Lunar Eclipse"},
    {20840123, "Lunar Eclipse"},
    {20870518, "Lunar Eclipse"},
    {20871110, "Lunar Eclipse"},
    {20880421, "Solar Eclipse"},
    // 2090s
    {20900316, "Lunar Eclipse"},
    {20900923, "Solar Eclipse"},
    {20910306, "Lunar Eclipse"},
    {20910829, "Lunar Eclipse"},
    {20920207, "Solar Eclipse"},
    {20930723, "Solar Eclipse"},
    {20940628, "Lunar Eclipse"},
    {20970511, "Solar Eclipse"},
    {20971021, "Lunar Eclipse"},
    {20980416, "Lunar Eclipse"},
    {20981010, "Lunar Eclipse"},
    {20990914, "Solar Eclipse"},
    // 2100s
    {21000904, "Solar Eclipse"},
    {21010214, "Lunar Eclipse"},
    {21010228, "Solar Eclipse"},
    {21010809, "Lunar Eclipse"},
    {21020203, "Lunar Eclipse"},
    {21020730, "Lunar Eclipse"},
    {21050528, "Lunar Eclipse"},
    {21051121, "Lunar Eclipse"},
    {21060503, "Solar Eclipse"},
    {21080327, "Lunar Eclipse"},
    {21090317, "Lunar Eclipse"},
    {21090909, "Lunar Eclipse"},
    // 2110s
    {21120709, "Lunar Eclipse"},
    {21130102, "Lunar Eclipse"},
    {21160427, "Lunar Eclipse"},
    {21161021, "Lunar Eclipse"},
    {21190311, "Solar Eclipse"},
    // 2120s
    {21200214, "Lunar Eclipse"},
    {21200809, "Lunar Eclipse"},
    {21230609, "Lunar Eclipse"},
    {21231203, "Lunar Eclipse"},
    {21260407, "Lunar Eclipse"},
    {0, NULL}  // End marker
};

// Check if date matches an eclipse
static const char* get_eclipse_message(int year, int month, int day) {
    int date = year * 10000 + month * 100 + day;
    for (int i = 0; eclipse_dates[i].message != NULL; i++) {
        if (eclipse_dates[i].date == date) {
            return eclipse_dates[i].message;
        }
    }
    return NULL;
}

// Get special message for a date, or NULL if none
static const char* get_special_message(int year, int month, int day) {
    // May 1st - rotate through messages based on year
    if (month == 5 && day == 1) {
        int count = 0;
        while (may_day_messages[count] != NULL) count++;
        return may_day_messages[year % count];
    }

    // Check for eclipse
    const char* eclipse_msg = get_eclipse_message(year, month, day);
    if (eclipse_msg != NULL) {
        return eclipse_msg;
    }

    // Regular lookup
    for (int i = 0; special_dates[i].message != NULL; i++) {
        if (special_dates[i].month == month && special_dates[i].day == day) {
            return special_dates[i].message;
        }
    }
    return NULL;
}

// Calculate equinox/solstice dates for a given year
// Uses Meeus algorithm - accurate to within a few minutes
// event: 0=Spring Equinox, 1=Summer Solstice, 2=Autumn Equinox, 3=Winter Solstice
static void get_solstice_equinox(int year, int event, int *month, int *day) {
    // Mean Julian Day for events at year 2000
    // From Jean Meeus "Astronomical Algorithms"
    double Y = (year - 2000) / 1000.0;
    double JDE0;

    switch (event) {
        case 0:  // Spring Equinox (March)
            JDE0 = 2451623.80984 + 365242.37404 * Y + 0.05169 * Y * Y
                   - 0.00411 * Y * Y * Y - 0.00057 * Y * Y * Y * Y;
            break;
        case 1:  // Summer Solstice (June)
            JDE0 = 2451716.56767 + 365241.62603 * Y + 0.00325 * Y * Y
                   + 0.00888 * Y * Y * Y - 0.00030 * Y * Y * Y * Y;
            break;
        case 2:  // Autumn Equinox (September)
            JDE0 = 2451810.21715 + 365242.01767 * Y - 0.11575 * Y * Y
                   + 0.00337 * Y * Y * Y + 0.00078 * Y * Y * Y * Y;
            break;
        case 3:  // Winter Solstice (December)
        default:
            JDE0 = 2451900.05952 + 365242.74049 * Y - 0.06223 * Y * Y
                   - 0.00823 * Y * Y * Y + 0.00032 * Y * Y * Y * Y;
            break;
    }

    // Convert Julian Day to calendar date
    // Add 0.5 to get to noon, then truncate
    int Z = (int)(JDE0 + 0.5);
    int A;
    if (Z < 2299161) {
        A = Z;
    } else {
        int alpha = (int)((Z - 1867216.25) / 36524.25);
        A = Z + 1 + alpha - alpha / 4;
    }
    int B = A + 1524;
    int C = (int)((B - 122.1) / 365.25);
    int D = (int)(365.25 * C);
    int E = (int)((B - D) / 30.6001);

    *day = B - D - (int)(30.6001 * E);
    if (E < 14) {
        *month = E - 1;
    } else {
        *month = E - 13;
    }
}

// Check if date is a solstice or equinox, return message or NULL
static const char* get_solar_event_message(int year, int month, int day) {
    int event_month, event_day;

    // Check Spring Equinox
    get_solstice_equinox(year, 0, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Spring Equinox";
    }

    // Check Summer Solstice
    get_solstice_equinox(year, 1, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Summer Solstice";
    }

    // Check Autumn Equinox
    get_solstice_equinox(year, 2, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Autumn Equinox";
    }

    // Check Winter Solstice
    get_solstice_equinox(year, 3, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Winter Solstice";
    }

    return NULL;
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Use HSI (internal 16MHz)
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 100;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3);
}

void GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    // LEDs
    g.Pin = LED1_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED1_PORT, &g);

    g.Pin = LED2_PIN;
    HAL_GPIO_Init(LED2_PORT, &g);

    // Button (PB1) - input with internal pull-up
    g.Pin = BTN_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_PORT, &g);

    // Power control pins (P-FET gates - HIGH = OFF, LOW = ON)
    g.Pin = RTC_PWR | NFC_PWR | EPD_PWR;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    // Start with all power gates OFF
    HAL_GPIO_WritePin(GPIOA, RTC_PWR, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, NFC_PWR, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, EPD_PWR, GPIO_PIN_SET);

    // LEDs off
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
}

void I2C_Init(void) {
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;  // PB6 = SCL, PB7 = SDA
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    hi2c.Instance = I2C1;
    hi2c.Init.ClockSpeed = 100000;
    hi2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c.Init.OwnAddress1 = 0;
    hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c);
}

void ADC_Init(void) {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure PB0 as analog input
    GPIO_InitTypeDef g = {0};
    g.Pin = VBAT_PIN;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(VBAT_PORT, &g);

    // Configure ADC1
    hadc.Instance = ADC1;
    hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.ScanConvMode = DISABLE;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.NbrOfConversion = 1;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc);

    // Configure channel 8 (PB0)
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = ADC_CHANNEL_8;
    ch.Rank = 1;
    ch.SamplingTime = ADC_SAMPLETIME_84CYCLES;  // Longer sampling for stable reading
    HAL_ADC_ConfigChannel(&hadc, &ch);
}

/**
 * Read battery voltage in millivolts.
 * Returns 0 if ADC read fails.
 */
uint32_t read_battery_mv(void) {
    HAL_ADC_Start(&hadc);
    if (HAL_ADC_PollForConversion(&hadc, 10) != HAL_OK) {
        return 0;
    }
    uint32_t adc_value = HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);

    // Convert ADC value to millivolts
    // ADC is 12-bit (0-4095), reference is 3.3V
    // voltage_mv = adc_value * 3300 / 4095
    return (adc_value * 3300) / 4095;
}

/**
 * Check if battery is critical.
 * Returns: 1 if critical (needs replacement), 0 if OK
 */
int is_battery_critical(void) {
    uint32_t mv = read_battery_mv();
    if (mv == 0) return 0;  // ADC error, assume OK
    return (mv < VBAT_CRITICAL_MV) ? 1 : 0;
}

/**
 * Get battery percentage (0-100%).
 * Based on 2x AA: 3.2V = 100%, 2.0V = 0%
 */
int get_battery_percent(void) {
    uint32_t mv = read_battery_mv();
    if (mv >= VBAT_FULL_MV) return 100;
    if (mv <= VBAT_EMPTY_MV) return 0;
    return (int)((mv - VBAT_EMPTY_MV) * 100 / (VBAT_FULL_MV - VBAT_EMPTY_MV));
}

// BCD to decimal conversion
static uint8_t bcd_to_dec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Decimal to BCD conversion
static uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

// Parse compile time __DATE__ and __TIME__ macros
static void parse_compile_time(int *year, int *month, int *day, int *hour, int *min, int *sec) {
    const char *date = __DATE__;
    const char *time_str = __TIME__;

    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {date[0], date[1], date[2], 0};
    const char *p = strstr(months, mon);
    *month = p ? ((p - months) / 3) + 1 : 1;

    *day = (date[4] == ' ') ? (date[5] - '0') : ((date[4] - '0') * 10 + (date[5] - '0'));
    *year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');

    *hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
    *min = (time_str[3] - '0') * 10 + (time_str[4] - '0');
    *sec = (time_str[6] - '0') * 10 + (time_str[7] - '0');
}

// Set DS3231 time
void ds3231_set_time(int year, int month, int day, int hour, int min, int sec) {
    uint8_t data[8];
    data[0] = 0x00;
    data[1] = dec_to_bcd(sec);
    data[2] = dec_to_bcd(min);
    data[3] = dec_to_bcd(hour);
    data[4] = 1;  // Day of week (not used)
    data[5] = dec_to_bcd(day);
    data[6] = dec_to_bcd(month);
    data[7] = dec_to_bcd(year - 2000);

    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, data, 8, 100);
}

// Read time from DS3231 and convert to Unix timestamp
time64_t ds3231_read_time(void) {
    uint8_t data[7];
    uint8_t reg = 0x00;

    if (HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, &reg, 1, 100) != HAL_OK) {
        return 0;
    }
    if (HAL_I2C_Master_Receive(&hi2c, DS3231_ADDR << 1, data, 7, 100) != HAL_OK) {
        return 0;
    }

    int min = bcd_to_dec(data[1] & 0x7F);
    int hour = bcd_to_dec(data[2] & 0x3F);
    int day = bcd_to_dec(data[4] & 0x3F);
    int month = bcd_to_dec(data[5] & 0x1F);
    int year = bcd_to_dec(data[6]) + 2000;

    return parse_margate_time(year, month, day, hour, min);
}

/**
 * Check if alarm flag is set and clear it.
 * Returns 1 if alarm was triggered (or first boot), 0 otherwise.
 */
int ds3231_check_and_clear_alarm(void) {
    uint8_t reg = DS3231_REG_STATUS;
    uint8_t status;

    // Read status register
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c, DS3231_ADDR << 1, &status, 1, 100);

    // Check A1F (bit 0) - Alarm 1 flag
    int alarm_triggered = (status & 0x01) ? 1 : 0;

    // Clear alarm flags (A1F and A2F) by writing 0 to bits 0 and 1
    uint8_t data[2] = {DS3231_REG_STATUS, status & 0xFC};
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, data, 2, 100);

    return alarm_triggered;
}

/**
 * Set Alarm 1 to trigger in N minutes.
 * Matches specific minutes and seconds value.
 */
void ds3231_set_alarm_minutes(int minutes_ahead) {
    // Read current time (seconds and minutes)
    uint8_t reg = 0x00;
    uint8_t time_data[3];
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c, DS3231_ADDR << 1, time_data, 3, 100);

    int current_sec = bcd_to_dec(time_data[0] & 0x7F);
    int current_min = bcd_to_dec(time_data[1] & 0x7F);
    int current_hour = bcd_to_dec(time_data[2] & 0x3F);

    // Calculate alarm time
    int total_minutes = current_hour * 60 + current_min + minutes_ahead;
    int alarm_hour = (total_minutes / 60) % 24;
    int alarm_min = total_minutes % 60;
    int alarm_sec = current_sec;  // Keep same seconds

    // Set Alarm 1 registers (0x07-0x0A)
    // A1M1=0 (seconds match), A1M2=0 (minutes match), A1M3=0 (hours match), A1M4=1 (day don't care)
    uint8_t alarm_data[5];
    alarm_data[0] = DS3231_REG_ALARM1;
    alarm_data[1] = dec_to_bcd(alarm_sec);       // Seconds: match this value
    alarm_data[2] = dec_to_bcd(alarm_min);       // Minutes: match this value
    alarm_data[3] = dec_to_bcd(alarm_hour);      // Hours: match this value (A1M3=0)
    alarm_data[4] = 0x80;                        // Day: A1M4=1 (don't care)

    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, alarm_data, 5, 100);

    // Enable Alarm 1 interrupt in control register
    // INTCN=1 (use INT pin), A1IE=1 (enable Alarm 1 interrupt)
    reg = DS3231_REG_CONTROL;
    uint8_t control;
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c, DS3231_ADDR << 1, &control, 1, 100);

    control |= 0x05;  // Set INTCN (bit 2) and A1IE (bit 0)
    control &= ~0x02; // Clear A2IE (bit 1) - we don't use Alarm 2

    uint8_t ctrl_data[2] = {DS3231_REG_CONTROL, control};
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, ctrl_data, 2, 100);
}

/**
 * Set Alarm 1 to trigger at next wake time based on time of day.
 *
 * Update intervals:
 * - Daytime (sunrise to sunset): every 20 minutes (:00, :20, :40)
 * - Twilight (4am-sunrise, sunset-midnight): every 30 minutes (:00, :30)
 * - Night (midnight to 4am): every 60 minutes (:00 only)
 *
 * Always aligns to boundaries so we hit :00 for hourly full refresh.
 */
void ds3231_set_alarm_next_interval(time64_t current_utc) {
    // Read current time from RTC
    uint8_t reg = 0x00;
    uint8_t time_data[3];
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c, DS3231_ADDR << 1, time_data, 3, 100);

    int current_min = bcd_to_dec(time_data[1] & 0x7F);
    int current_hour = bcd_to_dec(time_data[2] & 0x3F);

    // Get sunset time for today (in local Margate time)
    int sunrise_h, sunrise_m, sunset_h, sunset_m;
    calculate_sunrise_sunset(current_utc, &sunrise_h, &sunrise_m, &sunset_h, &sunset_m);

    // Determine interval based on current hour (RTC stores local Margate time)
    // Night (midnight-4am): 60 min, Twilight (4am-sunrise, sunset-midnight): 30 min, Day: 20 min
    int interval;
    if (current_hour >= 0 && current_hour < 4) {
        // Night: midnight to 4am - hourly updates
        interval = 60;
    } else if (current_hour >= sunset_h) {
        // Evening: after sunset to midnight - 30 minute intervals
        // Also check if we're past sunset minute when in the sunset hour
        if (current_hour == sunset_h && current_min < sunset_m) {
            // Still before sunset, use daytime interval
            interval = 20;
        } else {
            interval = 30;
        }
    } else if (current_hour < sunrise_h || (current_hour == sunrise_h && current_min < sunrise_m)) {
        // Early morning: 4am to sunrise - 30 minute intervals
        interval = 30;
    } else {
        // Daytime: after sunrise to sunset - 20 minute intervals
        interval = 20;
    }

    // Calculate next alarm time aligned to interval boundary
    int alarm_min, alarm_hour;

    if (interval == 60) {
        // Next hour
        alarm_min = 0;
        alarm_hour = (current_hour + 1) % 24;
    } else if (interval == 30) {
        // Next 30-minute boundary (:00, :30)
        int next_boundary = ((current_min / 30) + 1) * 30;
        alarm_min = next_boundary;
        alarm_hour = current_hour;
        if (alarm_min >= 60) {
            alarm_min = 0;
            alarm_hour = (alarm_hour + 1) % 24;
        }
    } else {
        // 20-minute boundary (:00, :20, :40)
        int next_boundary = ((current_min / 20) + 1) * 20;
        alarm_min = next_boundary;
        alarm_hour = current_hour;
        if (alarm_min >= 60) {
            alarm_min = 0;
            alarm_hour = (alarm_hour + 1) % 24;
        }
    }

    // Set Alarm 1 registers (0x07-0x0A)
    // Match seconds=0, minutes, hours; day don't care
    uint8_t alarm_data[5];
    alarm_data[0] = DS3231_REG_ALARM1;
    alarm_data[1] = 0x00;                        // Seconds: 0
    alarm_data[2] = dec_to_bcd(alarm_min);       // Minutes
    alarm_data[3] = dec_to_bcd(alarm_hour);      // Hours
    alarm_data[4] = 0x80;                        // Day: A1M4=1 (don't care)

    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, alarm_data, 5, 100);

    // Enable Alarm 1 interrupt
    reg = DS3231_REG_CONTROL;
    uint8_t control;
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, &reg, 1, 100);
    HAL_I2C_Master_Receive(&hi2c, DS3231_ADDR << 1, &control, 1, 100);

    control |= 0x05;  // Set INTCN and A1IE
    control &= ~0x02; // Clear A2IE

    uint8_t ctrl_data2[2] = {DS3231_REG_CONTROL, control};
    HAL_I2C_Master_Transmit(&hi2c, DS3231_ADDR << 1, ctrl_data2, 2, 100);
}

/**
 * Enter STOP mode while waiting for display BUSY pin (PA9) to go LOW.
 * Display power must remain ON during this time.
 * EXTI9 falling edge wakes the MCU when display is ready.
 */
void enter_stop_mode_wait_display(void) {
    // Configure PA9 (BUSY) as input
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    // Wait for BUSY to go HIGH (display starts refreshing)
    uint32_t wait_start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_RESET) {
        if ((HAL_GetTick() - wait_start) > 500) {
            return;  // Timeout
        }
    }

    // Poll for BUSY to go LOW (display done)
    // Use WFI to save power - wakes on SysTick every 1ms
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) {
        __WFI();
    }
}

// EXTI9_5 interrupt handler (for display BUSY pin)
void EXTI9_5_IRQHandler(void) {
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_9);
}

/**
 * Set all GPIOs to analog mode for lowest power consumption.
 * In analog mode, the input Schmitt trigger is disabled, preventing leakage.
 * Only exception: PA0 (WKUP pin) needs to stay functional.
 */
void GPIO_DeInit_For_Standby(void) {
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    // De-init I2C peripheral first (releases PB6/PB7)
    HAL_I2C_DeInit(&hi2c);
    __HAL_RCC_I2C1_CLK_DISABLE();

    // De-init SPI peripheral (releases PA5/PA7)
    __HAL_RCC_SPI1_CLK_DISABLE();

    // === DISABLE LSE OSCILLATOR ===
    // PC14/PC15 are controlled by backup domain, not normal GPIO
    // The LSE amplifier draws ~50µA if enabled with external signal
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();  // Unlock backup domain
    __HAL_RCC_LSE_CONFIG(RCC_LSE_OFF);  // Disable LSE oscillator
    HAL_PWR_DisableBkUpAccess();  // Lock backup domain

    // GPIOA: Set all pins to analog EXCEPT PA0 (WKUP pin)
    // PA0 = WKUP (keep as is - hardware handles it)
    // PA1-PA15 = analog
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
               GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 |
               GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
               GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &gpio);

    // GPIOB: All pins to analog (including I2C PB6/PB7)
    gpio.Pin = GPIO_PIN_All;
    HAL_GPIO_Init(GPIOB, &gpio);

    // GPIOC: All pins to analog (PC14/PC15 now safe after LSE disabled)
    HAL_GPIO_Init(GPIOC, &gpio);

    // GPIOD: All pins to analog
    HAL_GPIO_Init(GPIOD, &gpio);

    // Disable GPIO clocks to save a tiny bit more
    // (GPIOs retain state even with clock disabled)
    __HAL_RCC_GPIOB_CLK_DISABLE();
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOD_CLK_DISABLE();
    // Keep GPIOA enabled for WKUP functionality
}

// ============================================================================
// TIME SET MODE
// ============================================================================

// Button helper: returns 1 if button is pressed (active low)
static int button_pressed(void) {
    return HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_RESET;
}

// Wait for button release with debounce
static void wait_button_release(void) {
    while (button_pressed()) {
        HAL_Delay(10);
    }
    HAL_Delay(50);  // Debounce
}

// Check if button held for given milliseconds
static int button_held_for(uint32_t ms) {
    uint32_t start = HAL_GetTick();
    while (button_pressed()) {
        if ((HAL_GetTick() - start) >= ms) {
            return 1;
        }
        HAL_Delay(10);
    }
    return 0;
}

// Render time-set screen with underline under current digit
static void render_timeset_screen(int digits[], int current_digit) {
    display_clear();

    // Title and explanation
    display_draw_text(5, 14, "SET TIME", FONT_MEDIUM);
    display_draw_text(5, 28, "Enter digits. LED=ready.", FONT_SMALL);

    // Date line: YYYY-MM-DD
    char buf[32];
    snprintf(buf, sizeof(buf), "%d%d%d%d-%d%d-%d%d",
             digits[0], digits[1], digits[2], digits[3],
             digits[4], digits[5],
             digits[6], digits[7]);
    display_draw_text(5, 50, buf, FONT_MEDIUM);

    // Time line: HH:MM
    snprintf(buf, sizeof(buf), "%d%d:%d%d",
             digits[8], digits[9],
             digits[10], digits[11]);
    display_draw_text(5, 75, buf, FONT_MEDIUM);

    // Draw underline under current digit
    // Use display_get_text_width to measure actual positions
    int underline_x = 5;
    int underline_y = 52;  // Just below date line baseline
    int underline_w = 6;   // Underline width

    // Build prefix string up to current digit to measure width
    char prefix[16];
    if (current_digit < 4) {
        // Year digits 0-3: measure "N" chars
        snprintf(prefix, current_digit + 1, "%d%d%d%d", digits[0], digits[1], digits[2], digits[3]);
        prefix[current_digit] = '\0';
        underline_x = 5 + display_get_text_width(prefix, FONT_MEDIUM);
    } else if (current_digit < 6) {
        // Month digits 4-5: measure "YYYY-" + N chars
        int idx = current_digit - 4;
        snprintf(prefix, sizeof(prefix), "%d%d%d%d-", digits[0], digits[1], digits[2], digits[3]);
        if (idx > 0) {
            char tmp[4];
            snprintf(tmp, idx + 1, "%d%d", digits[4], digits[5]);
            tmp[idx] = '\0';
            strcat(prefix, tmp);
        }
        underline_x = 5 + display_get_text_width(prefix, FONT_MEDIUM);
    } else if (current_digit < 8) {
        // Day digits 6-7: measure "YYYY-MM-" + N chars
        int idx = current_digit - 6;
        snprintf(prefix, sizeof(prefix), "%d%d%d%d-%d%d-",
                 digits[0], digits[1], digits[2], digits[3],
                 digits[4], digits[5]);
        if (idx > 0) {
            char tmp[4];
            snprintf(tmp, idx + 1, "%d%d", digits[6], digits[7]);
            tmp[idx] = '\0';
            strcat(prefix, tmp);
        }
        underline_x = 5 + display_get_text_width(prefix, FONT_MEDIUM);
    } else {
        // Time digits 8-11
        underline_y = 77;  // Below time line baseline
        if (current_digit < 10) {
            // Hour digits 8-9
            int idx = current_digit - 8;
            prefix[0] = '\0';
            if (idx > 0) {
                snprintf(prefix, idx + 1, "%d%d", digits[8], digits[9]);
                prefix[idx] = '\0';
            }
            underline_x = 5 + display_get_text_width(prefix, FONT_MEDIUM);
        } else {
            // Minute digits 10-11: measure "HH:" + N chars
            int idx = current_digit - 10;
            snprintf(prefix, sizeof(prefix), "%d%d:", digits[8], digits[9]);
            if (idx > 0) {
                char tmp[4];
                snprintf(tmp, idx + 1, "%d%d", digits[10], digits[11]);
                tmp[idx] = '\0';
                strcat(prefix, tmp);
            }
            underline_x = 5 + display_get_text_width(prefix, FONT_MEDIUM);
        }
    }
    display_draw_line(underline_x, underline_y, underline_x + underline_w, underline_y, COLOR_BLACK, 2);

    // Instructions at bottom
    display_draw_text(5, 95, "Press=+1, Wait 2s=next", FONT_SMALL);
    display_draw_text(5, 108, "Hold 2s=0+next", FONT_SMALL);
    display_draw_text(5, 121, "Hold 4s=redo prev", FONT_SMALL);

    display_set_fast_full_mode();
    display_update(NULL);
    display_wait_ready();
}

/**
 * Time set mode - enter digits one by one
 * Short press = +1, Hold 2s = 0 and confirm, Hold 4s = redo previous, Wait 2s = confirm
 * LED on = ready for input, LED off during hold actions
 * Returns 1 if time was set, 0 if cancelled/timeout
 */
static int run_timeset_mode(void) {
    int digits[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // Start all at 0
    int current_digit = 0;

    // Show initial screen
    render_timeset_screen(digits, current_digit);

    while (current_digit < 12) {
        uint32_t last_activity = 0;  // 0 = no button pressed yet
        int button_was_pressed = 0;

        // LED on = ready for input
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);

        // Wait for input or timeout (only after first press)
        while (1) {
            if (button_pressed()) {
                // Check for long press actions
                uint32_t press_start = HAL_GetTick();
                int action_taken = 0;

                while (button_pressed()) {
                    uint32_t held_time = HAL_GetTick() - press_start;

                    // At 2s, turn LED off to indicate hold detected
                    if (held_time >= 2000 && held_time < 4000) {
                        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
                    }

                    // At 4s+, blink LED to indicate redo mode
                    if (held_time >= 4000) {
                        // Blink every 100ms
                        if ((held_time / 100) % 2 == 0) {
                            HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
                        } else {
                            HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
                        }
                    }

                    HAL_Delay(10);
                }

                // Check what action to take based on hold duration
                uint32_t total_held = HAL_GetTick() - press_start;

                // Hold 4s+ = go back to previous digit
                if (total_held >= 4000) {
                    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
                    if (current_digit > 0) {
                        current_digit--;
                        digits[current_digit] = 0;  // Reset previous digit
                    }
                    action_taken = 2;  // Redo action
                    render_timeset_screen(digits, current_digit);
                }

                // Check if released between 2s and 4s = set to 0 and confirm
                if (!action_taken && total_held >= 2000) {
                    // Hold 2-4s = set to 0 and confirm immediately
                    digits[current_digit] = 0;
                    current_digit++;
                    action_taken = 1;
                    if (current_digit < 12) {
                        render_timeset_screen(digits, current_digit);
                    }
                }

                if (action_taken) {
                    break;  // Move to next digit loop iteration
                }

                // Short press = increment (starts from current value)
                wait_button_release();
                digits[current_digit] = (digits[current_digit] + 1) % 10;
                last_activity = HAL_GetTick();
                button_was_pressed = 1;
            }

            // Check for 2 second timeout = confirm digit (only after button was pressed)
            if (button_was_pressed && (HAL_GetTick() - last_activity) > 2000) {
                // LED off briefly to indicate confirmation
                HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
                current_digit++;
                // Update screen after digit is confirmed
                if (current_digit < 12) {
                    render_timeset_screen(digits, current_digit);
                }
                break;
            }

            HAL_Delay(10);
        }
    }

    // LED off when done
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

    // All digits entered - construct time and set RTC
    int year = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
    int month = digits[4] * 10 + digits[5];
    int day = digits[6] * 10 + digits[7];
    int hour = digits[8] * 10 + digits[9];
    int min = digits[10] * 10 + digits[11];

    // Basic validation
    if (month < 1 || month > 12) month = 1;
    if (day < 1 || day > 31) day = 1;
    if (hour > 23) hour = 0;
    if (min > 59) min = 0;

    ds3231_set_time(year, month, day, hour, min, 0);

    // Show confirmation
    display_clear();
    display_draw_text(5, 30, "TIME SET!", FONT_MEDIUM);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    display_draw_text(5, 55, buf, FONT_MEDIUM);
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, min);
    display_draw_text(5, 80, buf, FONT_MEDIUM);
    display_set_fast_full_mode();
    display_update(NULL);
    display_wait_ready();

    HAL_Delay(2000);

    return 1;
}

/**
 * Debug mode - show 10 test frames to compare with simulator
 * Each frame displays for 10 seconds with a frame counter
 * Uses fixed timestamps that cover various moon phases and seasons
 */
static void run_frame_test(void) {
    // 10 test timestamps (UTC) spread across 2025
    // These should match the simulator test dates exactly
    static const struct {
        int year, month, day, hour, min;
        const char* desc;
    } test_frames[] = {
        {2025,  1,  1, 12, 0, "1/10 Jan 1"},      // New Year, waxing crescent
        {2025,  1, 13, 12, 0, "2/10 Jan 13"},     // Full moon
        {2025,  1, 29, 12, 0, "3/10 Jan 29"},     // New moon
        {2025,  3, 20, 12, 0, "4/10 Mar 20"},     // Spring equinox
        {2025,  6, 21, 12, 0, "5/10 Jun 21"},     // Summer solstice
        {2025,  8, 12, 12, 0, "6/10 Aug 12"},     // Perseids
        {2025,  9, 22, 12, 0, "7/10 Sep 22"},     // Autumn equinox
        {2025, 10, 31, 12, 0, "8/10 Oct 31"},     // Halloween
        {2025, 12, 21, 12, 0, "9/10 Dec 21"},     // Winter solstice
        {2025, 12, 24, 12, 0, "10/10 Dec 24"},    // Christmas Eve
    };

    for (int i = 0; i < 10; i++) {
        // Convert date to UTC timestamp
        time64_t test_time = parse_margate_time(
            test_frames[i].year,
            test_frames[i].month,
            test_frames[i].day,
            test_frames[i].hour,
            test_frames[i].min
        );

        // Get local time for message lookup
        struct tm *local = utc_to_margate(test_time);
        const char *message = NULL;

        // Check for special messages
        message = get_special_message(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
        if (message == NULL) {
            message = get_solar_event_message(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
        }
        // If no special message, show frame description
        if (message == NULL) {
            message = test_frames[i].desc;
        }

        // Render frame
        display_clear();
        render_tide_display(test_time, message);

        // First frame uses full refresh, rest use fast
        if (i == 0) {
            display_set_full_mode();
        } else {
            display_set_fast_full_mode();
        }
        display_update(NULL);
        display_wait_ready();

        // Blink LED to indicate frame number (i+1 blinks)
        for (int b = 0; b <= i; b++) {
            HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
            HAL_Delay(100);
            HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
            HAL_Delay(100);
        }

        // Wait 10 seconds total (minus LED blink time)
        HAL_Delay(10000 - (i + 1) * 200);
    }

    // Show "TEST COMPLETE" message
    display_clear();
    display_draw_text(10, 50, "FRAME TEST", FONT_LARGE);
    display_draw_text(10, 80, "COMPLETE", FONT_LARGE);
    display_set_fast_full_mode();
    display_update(NULL);
    display_wait_ready();
}

/**
 * Debug mode - show all messages in batches to verify none overflow
 * Messages are displayed centered like on the actual tide display
 */
static void run_message_debug(void) {
    // Collect all messages into an array
    const char* all_messages[100];
    int msg_count = 0;

    // Add special dates
    for (int i = 0; special_dates[i].message != NULL && msg_count < 100; i++) {
        all_messages[msg_count++] = special_dates[i].message;
    }

    // Add May Day messages
    for (int i = 0; may_day_messages[i] != NULL && msg_count < 100; i++) {
        all_messages[msg_count++] = may_day_messages[i];
    }

    // Add eclipse messages (just unique ones)
    all_messages[msg_count++] = "Solar Eclipse";
    all_messages[msg_count++] = "Lunar Eclipse";

    // Add solar events
    all_messages[msg_count++] = "Spring Equinox";
    all_messages[msg_count++] = "Summer Solstice";
    all_messages[msg_count++] = "Autumn Equinox";
    all_messages[msg_count++] = "Winter Solstice";

    // Add battery warning
    all_messages[msg_count++] = "Change batteries";

    // Display in batches of 10 messages per screen
    int msgs_per_page = 10;
    int line_height = 14;
    int start_y = 14;

    for (int page = 0; page * msgs_per_page < msg_count; page++) {
        display_clear();

        // Draw page header
        char header[32];
        snprintf(header, sizeof(header), "Messages %d-%d/%d",
                 page * msgs_per_page + 1,
                 (page + 1) * msgs_per_page > msg_count ? msg_count : (page + 1) * msgs_per_page,
                 msg_count);
        display_draw_text(5, start_y, header, FONT_SMALL);

        // Draw messages centered (like on tide display) using FONT_MED_SM
        for (int i = 0; i < msgs_per_page; i++) {
            int idx = page * msgs_per_page + i;
            if (idx >= msg_count) break;

            const char* msg = all_messages[idx];
            int msg_width = display_get_text_width(msg, FONT_MED_SM);
            int msg_x = (128 - msg_width) / 2;  // Center on 128px width
            int msg_y = start_y + 14 + (i * line_height);

            // Draw the message
            display_draw_text(msg_x, msg_y, msg, FONT_MED_SM);

            // Draw a line at 128px to show overflow
            display_draw_line(127, msg_y - 10, 127, msg_y + 2, COLOR_BLACK, 1);
        }

        display_set_fast_full_mode();
        display_update(NULL);
        display_wait_ready();

        // Wait 5 seconds or until button press
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 5000) {
            if (button_pressed()) {
                wait_button_release();
                break;
            }
            HAL_Delay(10);
        }
    }

    // Final screen - show longest message in actual tide display
    time64_t test_time = 1735689600;  // Jan 1 2025 12:00 UTC
    display_clear();
    render_tide_display(test_time, "Power to the people");
    display_set_fast_full_mode();
    display_update(NULL);
    display_wait_ready();
    HAL_Delay(5000);
}

/**
 * Enter STANDBY mode. MCU will wake on WKUP pin (PA0 rising edge).
 * This is the lowest power mode - 1.2V domain is powered off.
 * On wake-up, MCU performs a full reset (like power-on).
 */
void enter_standby_mode(void) {
    // === TURN OFF ALL EXTERNAL POWER GATES ===
    // P-FET gates: HIGH = OFF
    HAL_GPIO_WritePin(GPIOA, RTC_PWR, GPIO_PIN_SET);   // RTC off
    HAL_GPIO_WritePin(GPIOA, NFC_PWR, GPIO_PIN_SET);   // NFC off
    HAL_GPIO_WritePin(GPIOA, EPD_PWR, GPIO_PIN_SET);   // Display off

    // Small delay to ensure power gates are off
    HAL_Delay(1);

    // === SET ALL GPIOs TO ANALOG MODE ===
    // This prevents leakage through I2C pull-ups, SPI pins, etc.
    GPIO_DeInit_For_Standby();

    // Enable PWR clock (needed for standby configuration)
    __HAL_RCC_PWR_CLK_ENABLE();

    // Note: We don't disable DBGMCU->CR here - it only saves ~1-2µA
    // but makes programming very difficult (requires BOOT0 to recover)

    // Clear wake-up flag (must be cleared before entering standby)
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    // Enable WKUP pin (PA0) - rising edge will wake from standby
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    // Enter STANDBY mode
    // This function does not return - MCU resets on wake-up
    HAL_PWR_EnterSTANDBYMode();

    // We never reach here - MCU resets on wake-up
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();

    // LED1 on briefly to show we're awake
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);

    // Power on RTC (always on for alarm functionality)
    HAL_GPIO_WritePin(GPIOA, RTC_PWR, GPIO_PIN_RESET);
    HAL_Delay(10);

    // Initialize I2C
    I2C_Init();

    // Initialize ADC for battery monitoring
    ADC_Init();

    // Check if RTC is present
    if (HAL_I2C_IsDeviceReady(&hi2c, DS3231_ADDR << 1, 2, 100) != HAL_OK) {
        // RTC not found - blink LED1 as error
        while (1) {
            HAL_GPIO_TogglePin(LED1_PORT, LED1_PIN);
            HAL_Delay(200);
        }
    }

    // Check alarm flag - if not set, this is first boot after programming
    int alarm_was_set = ds3231_check_and_clear_alarm();

    // Track if display is already initialized and if time was manually set
    int display_initialized = 0;
    int time_was_set = 0;  // Show datetime after manual time set

    // On first boot (no alarm flag), check for time-set mode
    if (!alarm_was_set) {
        // Power on display early for time-set mode
        HAL_GPIO_WritePin(GPIOA, EPD_PWR, GPIO_PIN_RESET);
        HAL_Delay(10);
        display_set_fontset(FONTSET_HELVETICA);
        display_init_with_rotation(ROTATION_270);
        display_initialized = 1;

        // Check debug flags
        if (DEBUG_FRAME_TEST) {
            run_frame_test();
            while (1) { HAL_Delay(1000); }
        }

        if (DEBUG_MESSAGES) {
            run_message_debug();
            while (1) { HAL_Delay(1000); }
        }

        if (DEBUG_FAST_REFRESH) {
            // Continuous fast refresh test - cycles every 15 seconds
            // Shows cycle count so you can track ghosting buildup
            int cycle = 0;
            time64_t test_time = ds3231_read_time();

            while (1) {
                // Show cycle count as message
                static char cycle_msg[20];
                snprintf(cycle_msg, sizeof(cycle_msg), "Cycle %d", cycle);

                display_clear();
                render_tide_display(test_time, cycle_msg);

                // First cycle uses full refresh, rest use fast
                if (cycle == 0) {
                    display_set_full_mode();
                } else {
                    display_set_fast_full_mode();
                }
                display_update(NULL);
                display_wait_ready();

                // Blink LED to show cycle complete
                HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);
                HAL_Delay(100);
                HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);

                HAL_Delay(15000);  // Wait 15 seconds

                cycle++;
                test_time += 900;  // Add 15 min each cycle for varied display
            }
        }

        // Show prompt to enter time-set mode
        display_clear();
        display_draw_text(5, 40, "SET TIME?", FONT_MEDIUM);
        display_draw_text(5, 65, "Hold button 2s", FONT_SMALL);
        display_draw_text(5, 90, "Wait 5s to skip", FONT_SMALL);
        display_set_full_mode();  // Full refresh at boot to clear any ghosting
        display_update(NULL);
        display_wait_ready();

        // Wait up to 5 seconds for user to hold button
        uint32_t start = HAL_GetTick();
        int enter_timeset = 0;

        while ((HAL_GetTick() - start) < 5000) {
            if (button_pressed()) {
                // LED on to show button detected
                HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_SET);

                if (button_held_for(2000)) {
                    enter_timeset = 1;
                    wait_button_release();
                    break;
                }
            } else {
                HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
            }
            HAL_Delay(10);
        }

        if (enter_timeset) {
            run_timeset_mode();
            time_was_set = 1;  // Show datetime on first refresh after manual time set
        } else {
            // No time-set, use compile time as default
            // Convert from Zurich time (CET/CEST) to Margate time (GMT/BST)
            int year, month, day, hour, min, sec;
            parse_compile_time(&year, &month, &day, &hour, &min, &sec);
            // Subtract 1 hour for Zurich -> Margate conversion
            hour -= 1;
            if (hour < 0) {
                hour = 23;
                day -= 1;
                if (day < 1) {
                    month -= 1;
                    if (month < 1) {
                        month = 12;
                        year -= 1;
                    }
                    const int days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};
                    day = days_in_month[month - 1];
                }
            }
            ds3231_set_time(year, month, day, hour, min, sec);
        }

        // === First boot info screen ===
        // Show date/time and battery on one screen for 4 seconds
        time64_t boot_time = ds3231_read_time();
        struct tm *boot_local = utc_to_margate(boot_time);
        uint32_t mv = read_battery_mv();
        int percent = get_battery_percent();

        display_clear();
        char date_str[32], time_str[16], batt_str[16];
        snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d",
                 boot_local->tm_mday, boot_local->tm_mon + 1, boot_local->tm_year + 1900);
        snprintf(time_str, sizeof(time_str), "%02d:%02d",
                 boot_local->tm_hour, boot_local->tm_min);
        snprintf(batt_str, sizeof(batt_str), "%ld.%02ldV  %d%%", mv / 1000, (mv % 1000) / 10, percent);

        int date_w = display_get_text_width(date_str, FONT_LARGE);
        int time_w = display_get_text_width(time_str, FONT_LARGE);
        int batt_w = display_get_text_width(batt_str, FONT_MEDIUM);

        display_draw_text((DISPLAY_WIDTH - date_w) / 2, 45, date_str, FONT_LARGE);
        display_draw_text((DISPLAY_WIDTH - time_w) / 2, 75, time_str, FONT_LARGE);
        display_draw_text((DISPLAY_WIDTH - batt_w) / 2, 105, batt_str, FONT_MEDIUM);
        // Use fast full refresh for all updates (single flash, ~1s)
        display_set_fast_full_mode();

        display_update(NULL);
        display_wait_ready();
        HAL_Delay(3000);  // Brief pause before normal operation
    }

    // Read time from RTC
    time64_t current_time = ds3231_read_time();

    // Power on and initialize display (if not already done in time-set mode)
    if (!display_initialized) {
        HAL_GPIO_WritePin(GPIOA, EPD_PWR, GPIO_PIN_RESET);
        HAL_Delay(10);
        display_set_fontset(FONTSET_HELVETICA);
        display_init_with_rotation(ROTATION_270);
    }

    // Determine message for display
    struct tm *local = utc_to_margate(current_time);
    const char *message = NULL;

    // Check battery status first - highest priority warning
    if (is_battery_critical()) {
        message = "Change batteries";
    } else if (time_was_set) {
        // After manual time set, show datetime so user can verify
        static char datetime_str[32];
        snprintf(datetime_str, sizeof(datetime_str), "%02d/%02d/%04d %02d:%02d",
                 local->tm_mday, local->tm_mon + 1, local->tm_year + 1900,
                 local->tm_hour, local->tm_min);
        message = datetime_str;
    } else {
        // Check for special date message (holidays override solar events)
        message = get_special_message(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);

        // If no special date, check for solar events (equinox/solstice)
        if (message == NULL) {
            message = get_solar_event_message(local->tm_year + 1900,
                                              local->tm_mon + 1,
                                              local->tm_mday);
        }
    }

    // Daily full refresh at 3am to clear any accumulated ghosting
    // Fast refresh for all other updates
    if (local->tm_hour == 3 && local->tm_min == 0) {
        display_set_full_mode();  // Full OTP refresh clears ghosting
    } else {
        display_set_fast_full_mode();  // Fast refresh (~1s single flash)
    }

    // Render tide display (after setting refresh mode)
    display_clear();
    render_tide_display(current_time, message);

    // Send buffer to display (starts refresh)
    display_update(NULL);

    // Set next alarm based on time of day (15/20/60 min intervals)
    ds3231_set_alarm_next_interval(current_time);

    // LEDs off before sleep
    HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);

    // Clear any pending DS3231 alarm flags
    ds3231_check_and_clear_alarm();

    // Turn off RTC power - we've set the alarm, don't need it during display refresh
    HAL_GPIO_WritePin(GPIOA, RTC_PWR, GPIO_PIN_SET);

    // Wait for display refresh to complete (polls BUSY pin)
    enter_stop_mode_wait_display();

    // Display is now done - power it off (it will retain image)
    HAL_GPIO_WritePin(GPIOA, EPD_PWR, GPIO_PIN_SET);

    // Enter STANDBY mode - MCU will reset on wake-up (PA0 rising edge from DS3231)
    // This function does not return - main() runs again on wake
    enter_standby_mode();

    // Never reached
    while (1) {}
}

void SysTick_Handler(void) {
    HAL_IncTick();
}

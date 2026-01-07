/**
 * U8g2 STM32 Display Backend
 *
 * Implements display.h interface using u8g2 library with STM32 HAL SPI.
 * Uses IL3820/SSD1680 driver for 2.9" e-paper display (128x296).
 *
 * Pin mapping (from pinout.txt):
 *   PA3: Display VCC gate (P-FET, LOW=ON)
 *   PA4: CS
 *   PA5: CLK (SPI1_SCK)
 *   PA6: DC
 *   PA7: DIN (SPI1_MOSI)
 *   PA8: RST
 *   PA9: BUSY
 */

#include "display.h"
#include "u8g2.h"
#include "stm32f4xx_hal.h"
#include <string.h>

// SSD1680 setup function (custom driver based on tricolor BWR)
void u8g2_Setup_ssd1680_296x128_f(u8g2_t *u8g2, const u8g2_cb_t *rotation,
                                   u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb);

// Pin definitions
#define EPD_PWR_PIN    GPIO_PIN_3
#define EPD_CS_PIN     GPIO_PIN_4
#define EPD_CLK_PIN    GPIO_PIN_5
#define EPD_DC_PIN     GPIO_PIN_6
#define EPD_DIN_PIN    GPIO_PIN_7
#define EPD_RST_PIN    GPIO_PIN_8
#define EPD_BUSY_PIN   GPIO_PIN_9
#define EPD_PORT       GPIOA

// Global u8g2 instance
static u8g2_t u8g2;

// SPI handle
static SPI_HandleTypeDef hspi;

// Current settings
static int current_rotation = ROTATION_0;
static int current_fontset = FONTSET_HELVETICA;

// Forward declarations
static uint8_t u8x8_stm32_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
static uint8_t u8x8_stm32_byte_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

// Initialize SPI peripheral
static void spi_init(void) {
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure SPI pins: CLK (PA5), MOSI (PA7)
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = EPD_CLK_PIN | EPD_DIN_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);

    // Configure control pins: CS, DC, RST (outputs), BUSY (input), PWR (output)
    gpio.Pin = EPD_CS_PIN | EPD_DC_PIN | EPD_RST_PIN | EPD_PWR_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = EPD_BUSY_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    // Power off initially (P-FET, HIGH = OFF)
    HAL_GPIO_WritePin(EPD_PORT, EPD_PWR_PIN, GPIO_PIN_SET);

    // CS high (inactive)
    HAL_GPIO_WritePin(EPD_PORT, EPD_CS_PIN, GPIO_PIN_SET);

    // Configure SPI
    hspi.Instance = SPI1;
    hspi.Init.Mode = SPI_MODE_MASTER;
    hspi.Init.Direction = SPI_DIRECTION_2LINES;
    hspi.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi.Init.NSS = SPI_NSS_SOFT;
    hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  // ~2MHz at 16MHz clock
    hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi);
}

// GPIO and delay callback for u8g2
static uint8_t u8x8_stm32_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            // Already done in spi_init()
            break;

        case U8X8_MSG_DELAY_MILLI:
            HAL_Delay(arg_int);
            break;

        case U8X8_MSG_DELAY_10MICRO:
        case U8X8_MSG_DELAY_100NANO:
            // Short delays - just a few NOPs
            for (volatile int i = 0; i < arg_int * 10; i++) { __NOP(); }
            break;

        case U8X8_MSG_GPIO_CS:
            HAL_GPIO_WritePin(EPD_PORT, EPD_CS_PIN, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;

        case U8X8_MSG_GPIO_DC:
            HAL_GPIO_WritePin(EPD_PORT, EPD_DC_PIN, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;

        case U8X8_MSG_GPIO_RESET:
            HAL_GPIO_WritePin(EPD_PORT, EPD_RST_PIN, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;

        case U8X8_MSG_GPIO_E:
            // E-paper busy pin - return busy state
            // Note: Most e-paper displays are HIGH when busy
            break;

        default:
            return 0;
    }
    return 1;
}

// Hardware SPI byte callback for u8g2
static uint8_t u8x8_stm32_byte_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_SEND:
            HAL_SPI_Transmit(&hspi, (uint8_t *)arg_ptr, arg_int, 1000);
            break;

        case U8X8_MSG_BYTE_INIT:
            // SPI already initialized
            break;

        case U8X8_MSG_BYTE_SET_DC:
            HAL_GPIO_WritePin(EPD_PORT, EPD_DC_PIN, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            HAL_GPIO_WritePin(EPD_PORT, EPD_CS_PIN, GPIO_PIN_RESET);
            break;

        case U8X8_MSG_BYTE_END_TRANSFER:
            HAL_GPIO_WritePin(EPD_PORT, EPD_CS_PIN, GPIO_PIN_SET);
            break;

        default:
            return 0;
    }
    return 1;
}

void display_set_fontset(int fontset) {
    current_fontset = fontset;
}

void display_init_with_rotation(int rotation) {
    current_rotation = rotation;

    // Initialize SPI and GPIO
    spi_init();

    // Power on display (P-FET, LOW = ON)
    HAL_GPIO_WritePin(EPD_PORT, EPD_PWR_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);

    // Select rotation
    const u8g2_cb_t *cb;
    switch (rotation) {
        case ROTATION_90:  cb = U8G2_R1; break;
        case ROTATION_180: cb = U8G2_R2; break;
        case ROTATION_270: cb = U8G2_R3; break;
        default:           cb = U8G2_R0; break;
    }

    // Setup u8g2 with SSD1680 driver
    u8g2_Setup_ssd1680_296x128_f(&u8g2, cb, u8x8_stm32_byte_hw_spi, u8x8_stm32_gpio_and_delay);

    // Initialize display
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    display_clear();
}

void display_init(void) {
    display_init_with_rotation(ROTATION_0);
}

void display_clear(void) {
    u8g2_ClearBuffer(&u8g2);
}

void display_draw_pixel(int x, int y, uint8_t color) {
    u8g2_SetDrawColor(&u8g2, color == COLOR_BLACK ? 1 : 0);
    u8g2_DrawPixel(&u8g2, x, y);
}

void display_draw_line(int x0, int y0, int x1, int y1, uint8_t color, int width) {
    u8g2_SetDrawColor(&u8g2, color == COLOR_BLACK ? 1 : 0);

    if (width <= 1) {
        u8g2_DrawLine(&u8g2, x0, y0, x1, y1);
    } else {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int absdx = dx < 0 ? -dx : dx;
        int absdy = dy < 0 ? -dy : dy;

        // Draw exactly 'width' parallel lines (centered)
        for (int w = 0; w < width; w++) {
            int offset = w - (width - 1) / 2;
            if (absdx > absdy) {
                // Mostly horizontal: offset vertically
                u8g2_DrawLine(&u8g2, x0, y0 + offset, x1, y1 + offset);
            } else {
                // Mostly vertical: offset horizontally
                u8g2_DrawLine(&u8g2, x0 + offset, y0, x1 + offset, y1);
            }
        }
    }
}

void display_draw_rect(int x, int y, int w, int h, uint8_t fill, uint8_t outline) {
    if (fill != 255) {
        u8g2_SetDrawColor(&u8g2, fill == COLOR_BLACK ? 1 : 0);
        u8g2_DrawBox(&u8g2, x, y, w, h);
    }
    if (outline != 255) {
        u8g2_SetDrawColor(&u8g2, outline == COLOR_BLACK ? 1 : 0);
        u8g2_DrawFrame(&u8g2, x, y, w, h);
    }
}

void display_draw_circle(int cx, int cy, int radius, uint8_t fill, uint8_t outline) {
    if (fill != 255) {
        u8g2_SetDrawColor(&u8g2, fill == COLOR_BLACK ? 1 : 0);
        u8g2_DrawDisc(&u8g2, cx, cy, radius, U8G2_DRAW_ALL);
    }
    if (outline != 255) {
        u8g2_SetDrawColor(&u8g2, outline == COLOR_BLACK ? 1 : 0);
        u8g2_DrawCircle(&u8g2, cx, cy, radius, U8G2_DRAW_ALL);
    }
}

// Select font based on fontset and size
static void select_font(int font_size) {
    switch (current_fontset) {
        case FONTSET_PROFONT:
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_profont22_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_profont12_tr); break;
                case FONT_MED_SM: u8g2_SetFont(&u8g2, u8g2_font_profont11_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_profont10_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_profont11_tr); break;
            }
            break;
        case FONTSET_NCENR:
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_ncenR18_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_ncenR12_tr); break;
                case FONT_MED_SM: u8g2_SetFont(&u8g2, u8g2_font_ncenR10_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_ncenR08_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_ncenR10_tr); break;
            }
            break;
        default: // FONTSET_HELVETICA
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_helvB18_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_helvR12_tr); break;
                case FONT_MED_SM: u8g2_SetFont(&u8g2, u8g2_font_helvR10_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_helvR08_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_helvR10_tr); break;
            }
            break;
    }
}

void display_draw_text(int x, int y, const char* text, int font_size) {
    u8g2_SetDrawColor(&u8g2, 1);
    select_font(font_size);
    u8g2_DrawStr(&u8g2, x, y, text);
}

int display_get_text_width(const char* text, int font_size) {
    select_font(font_size);
    return u8g2_GetStrWidth(&u8g2, text);
}

void display_update(const char* filename) {
    (void)filename;  // Unused on STM32

    // Send buffer to e-paper display
    u8g2_SendBuffer(&u8g2);
}

/**
 * Wait for e-paper display to finish refreshing.
 * SSD1680: BUSY pin is HIGH when busy, LOW when ready.
 * Uses SLEEP mode between polls to save power (~15 sec refresh time).
 * Timeout after ~30 seconds.
 */
void display_wait_ready(void) {
    uint32_t timeout = 30000;  // 30 seconds max
    uint32_t start = HAL_GetTick();

    // Small delay to let BUSY go high first
    HAL_Delay(100);

    // Wait for BUSY pin to go LOW (ready)
    // Use SLEEP mode between polls to save power during refresh
    while (HAL_GPIO_ReadPin(EPD_PORT, EPD_BUSY_PIN) == GPIO_PIN_SET) {
        if ((HAL_GetTick() - start) > timeout) {
            break;  // Timeout, don't hang forever
        }
        // Enter SLEEP mode - wakes on any interrupt (SysTick every 1ms)
        // This saves power during the ~15 second display refresh
        __WFI();  // Wait For Interrupt - enters SLEEP mode
    }

    // Additional settling time after BUSY goes low
    HAL_Delay(100);
}

uint8_t* display_get_buffer(void) {
    return u8g2_GetBufferPtr(&u8g2);
}

// External functions from SSD1680 driver
extern void ssd1680_set_fast_refresh(int enable);
extern void ssd1680_set_refresh_mode(int mode);
extern void ssd1680_load_lut_and_refresh(u8g2_t *u8g2);  // Load LUT and trigger refresh

// Refresh mode constants (must match driver)
#define REFRESH_MODE_FULL        0
#define REFRESH_MODE_PARTIAL     1
#define REFRESH_MODE_FAST_FULL   2
#define REFRESH_MODE_MEDIUM      3
#define REFRESH_MODE_FAST_FULL_PASS1 4

// Base update - do a full refresh first
void display_base_update(void) {
    ssd1680_set_refresh_mode(REFRESH_MODE_FULL);
    u8g2_SendBuffer(&u8g2);
}

// Enable fast/partial refresh mode (~0.3s instead of 3s) - for small changes
void display_set_partial_mode(void) {
    ssd1680_set_refresh_mode(REFRESH_MODE_PARTIAL);
}

// Enable fast full refresh mode (~1s, single flash) - for full updates
void display_set_fast_full_mode(void) {
    ssd1680_set_refresh_mode(REFRESH_MODE_FAST_FULL);
}

// Enable fast full pass 1 mode (shorter timing, use before pass 2)
void display_set_fast_full_pass1_mode(void) {
    ssd1680_set_refresh_mode(REFRESH_MODE_FAST_FULL_PASS1);
}

// Refresh display without resending image data
void display_refresh_only(void) {
    ssd1680_load_lut_and_refresh(&u8g2);
}

// Return to full refresh mode (OTP waveform, multiple flashes)
void display_set_full_mode(void) {
    ssd1680_set_refresh_mode(REFRESH_MODE_FULL);
}

// Medium refresh mode (~1.5s, 2 flashes) - better ghosting clearance than fast
void display_set_medium_mode(void) {
    ssd1680_set_refresh_mode(REFRESH_MODE_MEDIUM);
}

// Do a partial/fast refresh
void display_partial_update(void) {
    // Fast refresh mode should already be set
    u8g2_SendBuffer(&u8g2);
}

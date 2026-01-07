/*
 * SSD1680 E-Paper Driver for u8g2
 * 2.9" 296x128 monochrome display (black/white)
 * Based on tricolor BWR driver with red RAM removed
 */

#include "u8g2.h"

// Display info - same as IL3820
static const u8x8_display_info_t u8x8_ssd1680_296x128_display_info = {
    /* chip_enable_level = */ 0,
    /* chip_disable_level = */ 1,
    /* post_chip_enable_wait_ns = */ 120,
    /* pre_chip_disable_wait_ns = */ 60,
    /* reset_pulse_width_ms = */ 100,
    /* post_reset_wait_ms = */ 100,
    /* sda_setup_time_ns = */ 50,
    /* sck_pulse_width_ns = */ 125,
    /* sck_clock_hz = */ 4000000UL,
    /* spi_mode = */ 0,
    /* i2c_bus_clock_100kHz = */ 4,
    /* data_setup_time_ns = */ 40,
    /* write_pulse_width_ns = */ 150,
    /* tile_width = */ 37,      // 296/8 = 37
    /* tile_height = */ 16,     // 128/8 = 16
    /* default_x_offset = */ 0,
    /* flipmode_x_offset = */ 0,
    /* pixel_width = */ 296,
    /* pixel_height = */ 128
};

// Power on sequence
static const uint8_t u8x8_ssd1680_powersave0_seq[] = {
    U8X8_START_TRANSFER(),
    U8X8_CA(0x22, 0xc0),  // enable clock and charge pump
    U8X8_C(0x20),         // execute
    U8X8_DLY(200),
    U8X8_DLY(100),
    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Power off sequence
static const uint8_t u8x8_ssd1680_powersave1_seq[] = {
    U8X8_START_TRANSFER(),
    U8X8_CA(0x22, 0x02),  // disable charge pump
    U8X8_C(0x20),
    U8X8_DLY(20),
    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Init sequence
static const uint8_t u8x8_ssd1680_init_seq[] = {
    U8X8_START_TRANSFER(),

    U8X8_C(0x01),
    U8X8_A(295 % 256), U8X8_A(295 / 256), U8X8_A(0),

    U8X8_CA(0x03, 0x75),  // Gate Driving voltage
    U8X8_CA(0x04, 0x0a),  // Source Driving voltage

    U8X8_CA(0x0b, 7),     // Delay of gate and source non overlap
    U8X8_CA(0x2c, 0xa8),  // VCOM value
    U8X8_CA(0x3a, 0x16),  // Dummy lines
    U8X8_CA(0x3b, 0x08),  // Gate time
    U8X8_CA(0x3c, 0x33),  // Border waveform

    U8X8_CA(0x11, 0x07),  // Data entry mode: x&y inc, x first

    U8X8_CAA(0x44, 0, 15),                          // RAM x start & end: 0 to 15 (128/8 - 1)
    U8X8_CAAAA(0x45, 0, 0, 295 & 255, 295 >> 8),    // RAM y start & end: 0 to 295

    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Refresh sequence - 0xF7 mode (full refresh)
// Standard full refresh - uses OTP waveform
static const uint8_t u8x8_ssd1680_refresh_seq[] = {
    U8X8_START_TRANSFER(),

    U8X8_CA(0x22, 0xF7),  // Display Update Control: Full refresh
    U8X8_C(0x20),         // Master Activation

    // Minimum wait for display refresh (~3s)
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),

    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Waveshare 2.9" V2 partial update LUT (159 bytes)
// From: https://github.com/waveshareteam/e-Paper/blob/master/STM32/STM32-F103ZET6/User/e-Paper/EPD_2in9_V2.c
static const uint8_t lut_partial[] = {
    0x0,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0A,0x0,0x0,0x0,0x0,0x0,0x2,
    0x1,0x0,0x0,0x0,0x0,0x0,0x0,
    0x1,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
    0x22,0x17,0x41,0xB0,0x32,0x36,
};

// Fast full refresh LUT - single pass 0x04,0x12
static const uint8_t lut_fast_full[] = {
    // Voltage levels (60 bytes) - standard voltages
    0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // Timing (90 bytes) - 0x04,0x12
    0x04,0x12,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // Config (9 bytes)
    0x24,0x42,0x22,0x22,0x23,0x32,0x00,0x00,0x00,
    0x22,0x17,0x41,0xAE,0x32,0x38,
};

// Medium refresh LUT - same as fast_full but with longer timing
static const uint8_t lut_medium[] = {
    // Voltage levels (60 bytes) - standard voltages
    0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // Timing (90 bytes) - single cycle
    0x02,0x10,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // Config (9 bytes)
    0x24,0x42,0x22,0x22,0x23,0x32,0x00,0x00,0x00,
    0x22,0x17,0x41,0xAE,0x32,0x38,
};

// Partial refresh FINAL activation sequence - Waveshare V2 method
// Called AFTER LUT loaded and image data written
// Uses 0x0F for partial display update (from Waveshare TurnOnDisplay_Partial)
static const uint8_t u8x8_ssd1680_fast_refresh_seq[] = {
    U8X8_START_TRANSFER(),

    U8X8_CA(0x22, 0x0F),  // Display Update Control: Partial mode activation
    U8X8_C(0x20),         // Master Activation

    // Wait for partial refresh (~0.3-0.5s)
    U8X8_DLY(250),
    U8X8_DLY(250),
    U8X8_DLY(250),

    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Convert tile for e-paper - invert data
static uint8_t *u8x8_convert_tile_for_ssd1680(uint8_t *t) {
    static uint8_t buf[8];
    for (uint8_t i = 0; i < 8; i++) {
        buf[i] = ~t[i];  // Invert
    }
    return buf;
}

// Draw tile - write to BOTH RAM banks for partial refresh support
// 0x24 = new image (black RAM), 0x26 = old image (red RAM used as "previous")
static void u8x8_ssd1680_draw_tile(u8x8_t *u8x8, uint8_t arg_int, void *arg_ptr) {
    uint16_t x;
    uint8_t c, page;
    uint8_t *ptr;
    uint8_t *converted;
    uint8_t arg_int_save = arg_int;

    u8x8_cad_StartTransfer(u8x8);

    // IL3820-style page calculation with Y-axis inversion
    page = u8x8->display_info->tile_height;
    page--;
    page -= (((u8x8_tile_t *)arg_ptr)->y_pos);
    page += 1;  // Display offset: shift by 1 on page axis (8 pixels)

    x = ((u8x8_tile_t *)arg_ptr)->x_pos;
    x *= 8;
    x += u8x8->x_offset;

    // Write to BLACK RAM (0x24) - the new image
    u8x8_cad_SendCmd(u8x8, 0x4f);  // set cursor column (Y)
    u8x8_cad_SendArg(u8x8, x & 255);
    u8x8_cad_SendArg(u8x8, x >> 8);

    u8x8_cad_SendCmd(u8x8, 0x4e);  // set cursor row (page)
    u8x8_cad_SendArg(u8x8, page);

    u8x8_cad_SendCmd(u8x8, 0x24);  // write to black RAM

    do {
        c = ((u8x8_tile_t *)arg_ptr)->cnt;
        ptr = ((u8x8_tile_t *)arg_ptr)->tile_ptr;
        do {
            converted = u8x8_convert_tile_for_ssd1680(ptr);
            u8x8_cad_SendData(u8x8, 8, converted);
            ptr += 8;
            c--;
        } while (c > 0);
        arg_int--;
    } while (arg_int > 0);

    // Also write same data to RED RAM (0x26) - needed for partial refresh
    // This tells the controller what the "old" image is
    u8x8_cad_SendCmd(u8x8, 0x4f);  // set cursor column (Y)
    u8x8_cad_SendArg(u8x8, x & 255);
    u8x8_cad_SendArg(u8x8, x >> 8);

    u8x8_cad_SendCmd(u8x8, 0x4e);  // set cursor row (page)
    u8x8_cad_SendArg(u8x8, page);

    u8x8_cad_SendCmd(u8x8, 0x26);  // write to red RAM (used as "previous" buffer)

    // Re-traverse the tiles for second write
    arg_int = arg_int_save;
    do {
        c = ((u8x8_tile_t *)arg_ptr)->cnt;
        ptr = ((u8x8_tile_t *)arg_ptr)->tile_ptr;
        do {
            converted = u8x8_convert_tile_for_ssd1680(ptr);
            u8x8_cad_SendData(u8x8, 8, converted);
            ptr += 8;
            c--;
        } while (c > 0);
        arg_int--;
    } while (arg_int > 0);

    u8x8_cad_EndTransfer(u8x8);
}

// Refresh modes
#define REFRESH_MODE_FULL        0   // Standard OTP waveform (multiple flashes, ~3s)
#define REFRESH_MODE_PARTIAL     1   // Partial update (no flash, ~0.3s) - for small changes
#define REFRESH_MODE_FAST_FULL   2   // Fast full update (0x10,0x20)
#define REFRESH_MODE_MEDIUM      3   // Medium refresh - same as fast full

// Track refresh mode
static int refresh_mode = REFRESH_MODE_FULL;
static int lut_loaded = 0;  // Track if custom LUT was loaded this frame

// Load a specific LUT and prepare display
static void load_lut(u8x8_t *u8x8, const uint8_t *lut, int lut_size, uint8_t border_waveform, int is_partial) {
    u8x8_cad_StartTransfer(u8x8);

    // Send LUT (command 0x32)
    u8x8_cad_SendCmd(u8x8, 0x32);
    for (int i = 0; i < lut_size; i++) {
        u8x8_cad_SendArg(u8x8, lut[i]);
    }

    if (is_partial) {
        // Partial mode needs command 0x37
        static const uint8_t cmd37_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00};
        u8x8_cad_SendCmd(u8x8, 0x37);
        for (int i = 0; i < 10; i++) {
            u8x8_cad_SendArg(u8x8, cmd37_data[i]);
        }
    }

    // Set border waveform
    u8x8_cad_SendCmd(u8x8, 0x3C);
    u8x8_cad_SendArg(u8x8, border_waveform);

    // Prepare display (0xC0 = enable clock + analog)
    u8x8_cad_SendCmd(u8x8, 0x22);
    u8x8_cad_SendArg(u8x8, 0xC0);
    u8x8_cad_SendCmd(u8x8, 0x20);

    u8x8_cad_EndTransfer(u8x8);

    // Wait for display to prepare
    u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_MILLI, 100);
}

// Trigger a display refresh and wait for completion
static void trigger_refresh(u8x8_t *u8x8) {
    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x22);
    u8x8_cad_SendArg(u8x8, 0xC7);  // Display update control
    u8x8_cad_SendCmd(u8x8, 0x20);  // Master activation
    u8x8_cad_EndTransfer(u8x8);

    // Wait for refresh to complete (~1s for fast refresh)
    // Must wait long enough for BUSY to go high then low again
    u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_MILLI, 250);
    u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_MILLI, 250);
    u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_MILLI, 250);
    u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_MILLI, 250);
    u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_MILLI, 250);
    u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_MILLI, 250);
}

// Load custom LUT for partial, fast-full, or medium refresh
// Must be called BEFORE writing image data
static void load_custom_lut(u8x8_t *u8x8) {
    const uint8_t *lut;
    int lut_size;
    uint8_t border_waveform;
    int is_partial = 0;

    if (refresh_mode == REFRESH_MODE_PARTIAL) {
        // Partial refresh setup
        lut = lut_partial;
        lut_size = sizeof(lut_partial);
        border_waveform = 0x80;  // Partial mode border
        is_partial = 1;
    } else if (refresh_mode == REFRESH_MODE_MEDIUM) {
        // Medium refresh setup (2 flashes)
        lut = lut_medium;
        lut_size = sizeof(lut_medium);
        border_waveform = 0x05;  // Same as fast full
    } else {
        // Fast full (0x10,0x20)
        lut = lut_fast_full;
        lut_size = sizeof(lut_fast_full);
        border_waveform = 0x05;
    }

    load_lut(u8x8, lut, lut_size, border_waveform, is_partial);
    lut_loaded = 1;
}

// Fast full refresh activation sequence - uses 0xC7 (same as Waveshare TurnOnDisplay)
// Note: Actual wait is done via BUSY pin in display_wait_ready(), these delays are just minimum
static const uint8_t u8x8_ssd1680_fast_full_refresh_seq[] = {
    U8X8_START_TRANSFER(),

    U8X8_CA(0x22, 0xC7),  // Display Update Control: Fast full refresh
    U8X8_C(0x20),         // Master Activation

    // Minimal delay - just let command settle, BUSY pin handles actual timing
    U8X8_DLY(100),

    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Main callback
uint8_t u8x8_d_ssd1680_296x128(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_DISPLAY_SETUP_MEMORY:
            u8x8_d_helper_display_setup_memory(u8x8, &u8x8_ssd1680_296x128_display_info);
            break;

        case U8X8_MSG_DISPLAY_INIT:
            u8x8_d_helper_display_init(u8x8);
            u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_init_seq);
            u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_powersave0_seq);
            refresh_mode = REFRESH_MODE_FULL;  // Start in full refresh mode
            break;

        case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
            if (arg_int == 0)
                u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_powersave0_seq);
            else
                u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_powersave1_seq);
            break;

        case U8X8_MSG_DISPLAY_DRAW_TILE:
            // For custom LUT modes: load LUT BEFORE first tile write
            if (refresh_mode != REFRESH_MODE_FULL && !lut_loaded) {
                load_custom_lut(u8x8);
            }
            u8x8_ssd1680_draw_tile(u8x8, arg_int, arg_ptr);
            break;

        case U8X8_MSG_DISPLAY_REFRESH:
            if (refresh_mode == REFRESH_MODE_PARTIAL) {
                // Partial: LUT loaded before tiles, use 0x0F activation
                u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_fast_refresh_seq);
                lut_loaded = 0;
            } else if (refresh_mode == REFRESH_MODE_FAST_FULL || refresh_mode == REFRESH_MODE_MEDIUM) {
                // Fast full modes: single pass with 0xC7 activation
                u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_fast_full_refresh_seq);
                lut_loaded = 0;
            } else {
                // Standard full refresh with OTP waveform
                u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_refresh_seq);
            }
            break;

        default:
            return 0;
    }
    return 1;
}

// Set refresh mode
// 0 = full (OTP waveform, multiple flashes, ~3s)
// 1 = partial (no flash, ~0.3s) - for small changes only
// 2 = fast full (single flash, ~1s) - full update but faster
void ssd1680_set_refresh_mode(int mode) {
    refresh_mode = mode;
    lut_loaded = 0;  // Reset LUT state when changing modes
}

// Legacy API - maps to partial mode
void ssd1680_set_fast_refresh(int enable) {
    refresh_mode = enable ? REFRESH_MODE_PARTIAL : REFRESH_MODE_FULL;
    lut_loaded = 0;
}

// Setup function
void u8g2_Setup_ssd1680_296x128_f(u8g2_t *u8g2, const u8g2_cb_t *rotation,
                                   u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb) {
    uint8_t tile_buf_height;
    uint8_t *buf;
    u8g2_SetupDisplay(u8g2, u8x8_d_ssd1680_296x128, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
    buf = u8g2_m_37_16_f(&tile_buf_height);
    u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}

/*
 * SSD1680 E-Paper Driver for u8g2
 * 2.9" 296x128 tri-color display (black/white/red)
 * Based on IL3820 driver
 *
 * This driver is for BWR (Black/White/Red) displays like WeAct 2.9" tri-color.
 * It writes to both black RAM (0x24) and color RAM (0x26) to suppress red.
 *
 * Key notes:
 * - Color RAM must be written with same offset as black RAM (per-tile)
 * - Uses 0xF7 update command for tri-color mode
 * - Refresh time: ~15 seconds
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

// Power on sequence - same as IL3820
static const uint8_t u8x8_ssd1680_powersave0_seq[] = {
    U8X8_START_TRANSFER(),
    U8X8_CA(0x22, 0xc0),  // enable clock and charge pump
    U8X8_C(0x20),         // execute
    U8X8_DLY(200),
    U8X8_DLY(100),
    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Power off sequence - same as IL3820
static const uint8_t u8x8_ssd1680_powersave1_seq[] = {
    U8X8_START_TRANSFER(),
    U8X8_CA(0x22, 0x02),  // disable charge pump
    U8X8_C(0x20),
    U8X8_DLY(20),
    U8X8_END_TRANSFER(),
    U8X8_END()
};

// Init sequence - same as IL3820 v2
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

// Refresh sequence - tri-color mode (0xF7)
static const uint8_t u8x8_ssd1680_refresh_seq[] = {
    U8X8_START_TRANSFER(),

    U8X8_CA(0x22, 0xF7),
    U8X8_C(0x20),         // Master Activation

    // Wait for refresh (~15 seconds for tri-color)
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

// Convert tile for e-paper - invert data
// SSD1680: 1 = white, 0 = black
// u8g2: 1 = pixel on (black), 0 = pixel off (white)
static uint8_t *u8x8_convert_tile_for_ssd1680(uint8_t *t) {
    static uint8_t buf[8];
    for (uint8_t i = 0; i < 8; i++) {
        buf[i] = ~t[i];  // Invert
    }
    return buf;
}

// Draw tile - write to black RAM and color RAM (with same offset)
static void u8x8_ssd1680_draw_tile(u8x8_t *u8x8, uint8_t arg_int, void *arg_ptr) {
    uint16_t x;
    uint8_t c, page;
    uint8_t *ptr;
    uint8_t *converted;

    u8x8_cad_StartTransfer(u8x8);

    page = u8x8->display_info->tile_height;
    page--;
    page -= (((u8x8_tile_t *)arg_ptr)->y_pos);
    page += 1;  // WeAct display offset: shift by 1 on page axis

    x = ((u8x8_tile_t *)arg_ptr)->x_pos;
    x *= 8;
    x += u8x8->x_offset;

    // Write to BLACK RAM (0x24)
    u8x8_cad_SendCmd(u8x8, 0x4f);  // set cursor column (Y)
    u8x8_cad_SendArg(u8x8, x & 255);
    u8x8_cad_SendArg(u8x8, x >> 8);

    u8x8_cad_SendCmd(u8x8, 0x4e);  // set cursor row (page)
    u8x8_cad_SendArg(u8x8, page);

    u8x8_cad_SendCmd(u8x8, 0x24);  // write to black RAM

    uint8_t arg_int_save = arg_int;
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

    // Write INVERTED data to COLOR RAM (0x26) - opposite of black RAM to suppress red
    u8x8_cad_SendCmd(u8x8, 0x4f);
    u8x8_cad_SendArg(u8x8, x & 255);
    u8x8_cad_SendArg(u8x8, x >> 8);

    u8x8_cad_SendCmd(u8x8, 0x4e);
    u8x8_cad_SendArg(u8x8, page);

    u8x8_cad_SendCmd(u8x8, 0x26);

    // Write all zeros to color RAM - no red anywhere
    static const uint8_t zeros[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    arg_int = arg_int_save;
    do {
        c = ((u8x8_tile_t *)arg_ptr)->cnt;
        do {
            u8x8_cad_SendData(u8x8, 8, (uint8_t *)zeros);
            c--;
        } while (c > 0);
        arg_int--;
    } while (arg_int > 0);

    u8x8_cad_EndTransfer(u8x8);
}

// Main callback
uint8_t u8x8_d_ssd1680_296x128_bwr(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_DISPLAY_SETUP_MEMORY:
            u8x8_d_helper_display_setup_memory(u8x8, &u8x8_ssd1680_296x128_display_info);
            break;

        case U8X8_MSG_DISPLAY_INIT:
            u8x8_d_helper_display_init(u8x8);
            u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_init_seq);
            u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_powersave0_seq);
            break;

        case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
            if (arg_int == 0)
                u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_powersave0_seq);
            else
                u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_powersave1_seq);
            break;

        case U8X8_MSG_DISPLAY_DRAW_TILE:
            u8x8_ssd1680_draw_tile(u8x8, arg_int, arg_ptr);
            break;

        case U8X8_MSG_DISPLAY_REFRESH:
            u8x8_cad_SendSequence(u8x8, u8x8_ssd1680_refresh_seq);
            break;

        default:
            return 0;
    }
    return 1;
}

// Setup function
void u8g2_Setup_ssd1680_296x128_bwr_f(u8g2_t *u8g2, const u8g2_cb_t *rotation,
                                       u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb) {
    uint8_t tile_buf_height;
    uint8_t *buf;
    u8g2_SetupDisplay(u8g2, u8x8_d_ssd1680_296x128_bwr, u8x8_cad_011, byte_cb, gpio_and_delay_cb);
    buf = u8g2_m_37_16_f(&tile_buf_height);
    u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}

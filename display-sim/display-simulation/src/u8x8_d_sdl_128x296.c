/**
 * SDL Display Driver for 128x296 E-Paper Simulation
 *
 * Based on u8g2's SDL display drivers.
 * Simulates a 2.9" e-paper display (128x296 pixels, 1-bit).
 */

#include "u8g2.h"

#ifndef NO_SDL
#include <SDL2/SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>

// Display dimensions
#define EPAPER_WIDTH  128
#define EPAPER_HEIGHT 296

// SDL globals
#ifndef NO_SDL
static SDL_Window *sdl_window = NULL;
static SDL_Surface *sdl_screen = NULL;
#endif

static int sdl_scale = 2;  // Pixel scaling for visibility
static uint32_t sdl_color_white;
static uint32_t sdl_color_black;

// PNG export support
static uint8_t *png_buffer = NULL;
static const char *png_filename = NULL;

// Forward declarations
int u8g_sdl_get_key(void);

static void sdl_set_pixel_raw(int x, int y, int is_black)
{
    // Store pixels directly to PNG buffer
    // x, y are already in correct coordinate system (y=0 at top)
#ifndef NO_SDL
    if (x < 0 || x >= EPAPER_WIDTH || y < 0 || y >= EPAPER_HEIGHT)
        return;

    uint32_t color = is_black ? sdl_color_black : sdl_color_white;

    for (int i = 0; i < sdl_scale; i++) {
        for (int j = 0; j < sdl_scale; j++) {
            int px = x * sdl_scale + j;
            int py = y * sdl_scale + i;
            uint32_t *pixels = (uint32_t *)sdl_screen->pixels;
            pixels[py * (EPAPER_WIDTH * sdl_scale) + px] = color;
        }
    }
#else
    (void)x; (void)is_black;
#endif

    if (png_buffer && x >= 0 && x < EPAPER_WIDTH && y >= 0 && y < EPAPER_HEIGHT) {
        png_buffer[y * EPAPER_WIDTH + x] = is_black ? 0 : 255;
    }
}

static void sdl_set_8pixel(int x, int y, uint8_t pixel)
{
    // Render 8 vertical pixels from tile data
    // For u8g2_ll_hvline_vertical_top_lsb: bit 0 = top of tile
    for (int i = 0; i < 8; i++) {
        int is_black = (pixel >> i) & 1;
        sdl_set_pixel_raw(x, y + i, is_black);
    }
}

static void sdl_set_multiple_8pixel(int x, int y, int cnt, uint8_t *pixel)
{
    while (cnt > 0) {
        sdl_set_8pixel(x, y, *pixel);
        x++;
        pixel++;
        cnt--;
    }
}

static int sdl_initialized = 0;

static void sdl_init(void)
{
    if (sdl_initialized) return;  // Only initialize once!
    sdl_initialized = 1;

    // Allocate PNG buffer
    png_buffer = (uint8_t *)malloc(EPAPER_WIDTH * EPAPER_HEIGHT);
    if (png_buffer) {
        memset(png_buffer, 255, EPAPER_WIDTH * EPAPER_HEIGHT);  // White background
    }

#ifndef NO_SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return;
    }

    sdl_window = SDL_CreateWindow(
        "Tide Display (u8g2 Simulation)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        EPAPER_WIDTH * sdl_scale, EPAPER_HEIGHT * sdl_scale,
        SDL_WINDOW_SHOWN
    );

    if (!sdl_window) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        return;
    }

    sdl_screen = SDL_GetWindowSurface(sdl_window);
    if (!sdl_screen) {
        fprintf(stderr, "SDL surface failed: %s\n", SDL_GetError());
        return;
    }

    // E-paper colors: white background, black foreground
    sdl_color_white = SDL_MapRGB(sdl_screen->format, 255, 255, 255);
    sdl_color_black = SDL_MapRGB(sdl_screen->format, 0, 0, 0);

    // Fill with white
    SDL_FillRect(sdl_screen, NULL, sdl_color_white);
    SDL_UpdateWindowSurface(sdl_window);

    atexit(SDL_Quit);
#endif
}

// SDL key handling
int u8g_sdl_get_key(void)
{
#ifndef NO_SDL
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return 'q';
        }
        if (event.type == SDL_KEYDOWN) {
            return event.key.keysym.sym;
        }
    }
#endif
    return -1;
}

// GPIO callback for u8g2
static uint8_t u8x8_gpio_sdl(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_int;
    (void)arg_ptr;

    switch (msg) {
        case U8X8_MSG_DELAY_MILLI:
#ifndef NO_SDL
            SDL_Delay(arg_int);
#endif
            break;
    }
    return 1;
}

// Display info for 128x296
static const u8x8_display_info_t u8x8_sdl_128x296_info = {
    /* chip_enable_level = */ 0,
    /* chip_disable_level = */ 1,
    /* post_chip_enable_wait_ns = */ 0,
    /* pre_chip_disable_wait_ns = */ 0,
    /* reset_pulse_width_ms = */ 0,
    /* post_reset_wait_ms = */ 0,
    /* sda_setup_time_ns = */ 0,
    /* sck_pulse_width_ns = */ 0,
    /* sck_clock_hz = */ 4000000UL,
    /* spi_mode = */ 0,
    /* i2c_bus_clock_100kHz = */ 0,
    /* data_setup_time_ns = */ 0,
    /* write_pulse_width_ns = */ 0,
    /* tile_width = */ 16,      // 128 / 8 = 16 tiles
    /* tile_height = */ 37,     // 296 / 8 = 37 tiles
    /* default_x_offset = */ 0,
    /* flipmode_x_offset = */ 0,
    /* pixel_width = */ 128,
    /* pixel_height = */ 296
};

// Display callback
uint8_t u8x8_d_sdl_128x296(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    int x, y;  // Use int to avoid overflow for displays > 256 pixels
    uint8_t c;
    uint8_t *ptr;

    switch (msg) {
        case U8X8_MSG_DISPLAY_SETUP_MEMORY:
            u8x8_d_helper_display_setup_memory(u8x8, &u8x8_sdl_128x296_info);
            sdl_init();
            break;

        case U8X8_MSG_DISPLAY_INIT:
            u8x8_d_helper_display_init(u8x8);
            break;

        case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
        case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
        case U8X8_MSG_DISPLAY_SET_CONTRAST:
            break;

        case U8X8_MSG_DISPLAY_DRAW_TILE:
            x = ((u8x8_tile_t *)arg_ptr)->x_pos;
            x *= 8;
            x += u8x8->x_offset;

            y = ((u8x8_tile_t *)arg_ptr)->y_pos;
            y *= 8;

            do {
                c = ((u8x8_tile_t *)arg_ptr)->cnt;
                ptr = ((u8x8_tile_t *)arg_ptr)->tile_ptr;
                sdl_set_multiple_8pixel(x, y, c * 8, ptr);
                arg_int--;
                x += c * 8;
            } while (arg_int > 0);

#ifndef NO_SDL
            SDL_UpdateWindowSurface(sdl_window);
#endif
            break;

        default:
            return 0;
    }
    return 1;
}

// Setup functions
void u8x8_Setup_SDL_128x296(u8x8_t *u8x8)
{
    u8x8_SetupDefaults(u8x8);
    u8x8->display_cb = u8x8_d_sdl_128x296;
    u8x8->gpio_and_delay_cb = u8x8_gpio_sdl;
    u8x8_SetupMemory(u8x8);
}

void u8g2_SetupBuffer_SDL_128x296(u8g2_t *u8g2, const u8g2_cb_t *u8g2_cb)
{
    static uint8_t buf[128 * 37];  // Full buffer: 128 * (296/8) = 128 * 37 = 4736 bytes

    u8x8_Setup_SDL_128x296(u8g2_GetU8x8(u8g2));
    u8g2_SetupBuffer(u8g2, buf, 37, u8g2_ll_hvline_vertical_top_lsb, u8g2_cb);
}

// PNG export using stb_image_write
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void u8g2_sdl_set_png_filename(const char *filename)
{
    png_filename = filename;
}

// Current display rotation setting
static int display_rotation = 0;  // 0, 90, 180, 270

void u8g2_sdl_set_rotation(int rotation) {
    display_rotation = rotation;
}

void u8g2_sdl_save_png(void)
{
    if (png_filename && png_buffer) {
        if (display_rotation == 90) {
            // When using U8G2_R1 (90° rotation), the logical display is 296x128 landscape
            // but the buffer is stored as 128x296 portrait. Rotate 90° CCW for readable PNG.
            // Output: 296x128 (landscape, text readable)
            uint8_t *rotated = (uint8_t *)malloc(EPAPER_WIDTH * EPAPER_HEIGHT);
            if (rotated) {
                for (int y = 0; y < EPAPER_HEIGHT; y++) {
                    for (int x = 0; x < EPAPER_WIDTH; x++) {
                        // 90° CCW: src(x,y) -> dst(y, width-1-x)
                        int src_idx = y * EPAPER_WIDTH + x;
                        int dst_x = y;
                        int dst_y = EPAPER_WIDTH - 1 - x;
                        int dst_idx = dst_y * EPAPER_HEIGHT + dst_x;
                        rotated[dst_idx] = png_buffer[src_idx];
                    }
                }
                stbi_write_png(png_filename, EPAPER_HEIGHT, EPAPER_WIDTH, 1, rotated, EPAPER_HEIGHT);
                free(rotated);
            }
        } else {
            stbi_write_png(png_filename, EPAPER_WIDTH, EPAPER_HEIGHT, 1, png_buffer, EPAPER_WIDTH);
        }
    }
}

uint8_t *u8g2_sdl_get_buffer(void)
{
    return png_buffer;
}

void u8g2_sdl_clear_buffer(void)
{
    if (png_buffer) {
        memset(png_buffer, 255, EPAPER_WIDTH * EPAPER_HEIGHT);  // White
    }
#ifndef NO_SDL
    if (sdl_screen) {
        SDL_FillRect(sdl_screen, NULL, sdl_color_white);
    }
#endif
}

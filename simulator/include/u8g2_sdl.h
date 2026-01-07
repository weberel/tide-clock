/**
 * U8g2 SDL Simulation Header
 *
 * Provides setup functions for 128x296 e-paper simulation
 */

#ifndef U8G2_SDL_H
#define U8G2_SDL_H

#include "u8g2.h"

// Setup u8g2 with SDL backend for 128x296 display
void u8g2_SetupBuffer_SDL_128x296(u8g2_t *u8g2, const u8g2_cb_t *u8g2_cb);

// Get key from SDL (for interactive mode)
int u8g_sdl_get_key(void);

// PNG export functions
void u8g2_sdl_set_png_filename(const char *filename);
void u8g2_sdl_set_rotation(int rotation);
void u8g2_sdl_save_png(void);
uint8_t *u8g2_sdl_get_buffer(void);
void u8g2_sdl_clear_buffer(void);

#endif // U8G2_SDL_H

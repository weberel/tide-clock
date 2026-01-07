#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Display dimensions (2.9" e-paper)
#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 296

// Colors
#define COLOR_BLACK 0
#define COLOR_WHITE 1

// Font sizes
#define FONT_LARGE  0
#define FONT_MEDIUM 1
#define FONT_SMALL  2
#define FONT_TINY   3

// Font sets (different font families)
#define FONTSET_HELVETICA 0
#define FONTSET_PROFONT   1
#define FONTSET_NCENR     2
#define FONTSET_COURR     3
#define FONTSET_HAXR      4
#define FONTSET_PIXELLE   5

// Rotation options (degrees clockwise)
#define ROTATION_0   0
#define ROTATION_90  1
#define ROTATION_180 2
#define ROTATION_270 3

// Initialize display with rotation (0, 90, 180, or 270 degrees)
void display_init_with_rotation(int rotation);

// Initialize display (no rotation)
void display_init(void);

// Clear display (fill with white)
void display_clear(void);

// Drawing primitives
void display_draw_pixel(int x, int y, uint8_t color);
void display_draw_line(int x0, int y0, int x1, int y1, uint8_t color, int width);
void display_draw_rect(int x, int y, int w, int h, uint8_t fill, uint8_t outline);
void display_draw_circle(int cx, int cy, int radius, uint8_t fill, uint8_t outline);

// Text rendering
void display_draw_text(int x, int y, const char* text, int font_size);
int display_get_text_width(const char* text, int font_size);
void display_set_fontset(int fontset);

// Finalize and output
// PC: saves to PNG file
// STM32: refreshes e-paper display
void display_update(const char* filename);

// Get raw buffer (for STM32 SPI transfer)
uint8_t* display_get_buffer(void);

#endif // DISPLAY_H

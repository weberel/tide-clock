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
#define FONT_MED_SM 2  // Between medium and small (10pt)
#define FONT_SMALL  3
#define FONT_TINY   4

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

// Wait for e-paper display to finish refreshing (STM32 only)
void display_wait_ready(void);

// Get raw buffer (for STM32 SPI transfer)
uint8_t* display_get_buffer(void);

// Partial refresh support (STM32 only)
// Display base image and write to both RAM banks (required before partial updates)
void display_base_update(void);

// Enable partial mode for fast updates (~300ms vs ~2s)
// Call this ONCE after display_base_update(), before doing partial updates
void display_set_partial_mode(void);

// Enable fast full refresh mode (~1s, single flash)
// Full screen update but faster than standard full refresh
void display_set_fast_full_mode(void);

// Enable fast full pass 1 mode (shorter timing, use before pass 2)
// For two-pass refresh: set pass1, update, wait, set fast_full, refresh_only, wait
void display_set_fast_full_pass1_mode(void);

// Refresh display without resending image data (for second pass of two-pass refresh)
// Use after display_wait_ready() from first pass, with new mode already set
void display_refresh_only(void);

// Return to full refresh mode (better contrast, clears ghosting)
void display_set_full_mode(void);

// Medium refresh mode (~1.5s, 2 flashes) - better ghosting clearance than fast
void display_set_medium_mode(void);

// Do a partial refresh (must call display_set_partial_mode first)
void display_partial_update(void);

#endif // DISPLAY_H

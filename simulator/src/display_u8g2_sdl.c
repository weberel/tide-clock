/**
 * U8g2 SDL Display Backend
 *
 * Implements display.h interface using u8g2 library with SDL backend.
 * This provides pixel-perfect simulation that matches STM32 output.
 */

#include "display.h"
#include "u8g2.h"
#include "u8g2_sdl.h"
#include <string.h>
#include <stdlib.h>

// Global u8g2 instance
static u8g2_t u8g2;

// Current rotation setting
static int current_rotation = ROTATION_0;

// Current font set
static int current_fontset = FONTSET_HELVETICA;

void display_set_fontset(int fontset) {
    current_fontset = fontset;
}

void display_init_with_rotation(int rotation) {
    current_rotation = rotation;

    // Select u8g2 rotation callback based on rotation
    const u8g2_cb_t *cb;
    switch (rotation) {
        case ROTATION_90:
            cb = U8G2_R1;
            break;
        case ROTATION_180:
            cb = U8G2_R2;
            break;
        case ROTATION_270:
            cb = U8G2_R3;
            break;
        default:
            cb = U8G2_R0;
            break;
    }

    // Tell SDL driver about rotation for PNG export
    u8g2_sdl_set_rotation(rotation * 90);

    u8g2_SetupBuffer_SDL_128x296(&u8g2, cb);
    u8x8_InitDisplay(u8g2_GetU8x8(&u8g2));
    u8x8_SetPowerSave(u8g2_GetU8x8(&u8g2), 0);
    display_clear();
}

void display_init(void) {
    display_init_with_rotation(ROTATION_0);
}

void display_clear(void) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_sdl_clear_buffer();  // Also clear our PNG buffer
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
        // Draw thick line by offsetting
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);

        for (int w = -width/2; w <= width/2; w++) {
            if (dx > dy) {
                // More horizontal
                u8g2_DrawLine(&u8g2, x0, y0 + w, x1, y1 + w);
            } else {
                // More vertical
                u8g2_DrawLine(&u8g2, x0 + w, y0, x1 + w, y1);
            }
        }
    }
}

void display_draw_rect(int x, int y, int w, int h, uint8_t fill, uint8_t outline) {
    // Fill first
    if (fill != 255) {
        u8g2_SetDrawColor(&u8g2, fill == COLOR_BLACK ? 1 : 0);
        u8g2_DrawBox(&u8g2, x, y, w, h);
    }

    // Then outline
    if (outline != 255) {
        u8g2_SetDrawColor(&u8g2, outline == COLOR_BLACK ? 1 : 0);
        u8g2_DrawFrame(&u8g2, x, y, w, h);
    }
}

void display_draw_circle(int cx, int cy, int radius, uint8_t fill, uint8_t outline) {
    // Fill first (disc)
    if (fill != 255) {
        u8g2_SetDrawColor(&u8g2, fill == COLOR_BLACK ? 1 : 0);
        u8g2_DrawDisc(&u8g2, cx, cy, radius, U8G2_DRAW_ALL);
    }

    // Then outline (circle)
    if (outline != 255) {
        u8g2_SetDrawColor(&u8g2, outline == COLOR_BLACK ? 1 : 0);
        u8g2_DrawCircle(&u8g2, cx, cy, radius, U8G2_DRAW_ALL);
    }
}

// Helper to select font based on current fontset and size
static void select_font(int font_size) {
    switch (current_fontset) {
        case FONTSET_PROFONT:
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_profont22_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_profont12_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_profont10_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_profont11_tr); break;
            }
            break;
        case FONTSET_NCENR:
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_ncenR18_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_ncenR12_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_ncenR08_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_ncenR10_tr); break;
            }
            break;
        case FONTSET_COURR:
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_courR18_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_courR12_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_courR08_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_courR10_tr); break;
            }
            break;
        case FONTSET_HAXR:
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_4x6_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr); break;
            }
            break;
        case FONTSET_PIXELLE:
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_pixelle_micro_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_pixelle_micro_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_pixelle_micro_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_4x6_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_pixelle_micro_tr); break;
            }
            break;
        default: // FONTSET_HELVETICA
            switch (font_size) {
                case FONT_LARGE:  u8g2_SetFont(&u8g2, u8g2_font_helvB18_tr); break;
                case FONT_MEDIUM: u8g2_SetFont(&u8g2, u8g2_font_helvR12_tr); break;
                case FONT_SMALL:  u8g2_SetFont(&u8g2, u8g2_font_helvR08_tr); break;
                case FONT_TINY:   u8g2_SetFont(&u8g2, u8g2_font_5x7_tr); break;
                default:          u8g2_SetFont(&u8g2, u8g2_font_helvR10_tr); break;
            }
            break;
    }
}

void display_draw_text(int x, int y, const char* text, int font_size) {
    u8g2_SetDrawColor(&u8g2, 1);  // Text always black
    select_font(font_size);
    u8g2_DrawStr(&u8g2, x, y, text);
}

int display_get_text_width(const char* text, int font_size) {
    select_font(font_size);
    return u8g2_GetStrWidth(&u8g2, text);
}

void display_update(const char* filename) {
    // Send buffer to SDL display
    u8g2_SendBuffer(&u8g2);

    // Also save as PNG if filename provided
    if (filename) {
        u8g2_sdl_set_png_filename(filename);
        u8g2_sdl_save_png();
    }
}

uint8_t* display_get_buffer(void) {
    return u8g2_GetBufferPtr(&u8g2);
}

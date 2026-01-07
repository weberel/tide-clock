/**
 * Display Rendering
 *
 * Drawing helpers and main render function for tide display.
 */

#include "render.h"
#include "display.h"
#include "tide.h"
#include "astro.h"
#include "timezone.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void draw_moon_phase(int cx, int cy, int radius, float phase) {
    // Phase: 0=new (dark), 0.5=full (bright), 1=new again
    //
    // Illumination fraction: 0 at new, 1 at full, 0 at new again
    float illum = 1.0f - fabsf(2.0f * phase - 1.0f);  // triangle wave 0->1->0

    // Which side is lit: right during waxing (phase<0.5), left during waning
    int waxing = (phase < 0.5f);

    // Draw each scanline
    for (int y = -radius; y <= radius; y++) {
        int x_extent = (int)sqrtf(radius * radius - y * y);
        if (x_extent == 0) continue;

        int left_edge = cx - x_extent;
        int right_edge = cx + x_extent;
        int width = 2 * x_extent;

        // How many pixels are lit on this scanline
        int lit_pixels = (int)(illum * width);

        if (waxing) {
            // Right side lit: draw dark on left, bright on right
            int dark_end = right_edge - lit_pixels;
            display_draw_line(left_edge, cy + y, right_edge, cy + y, COLOR_BLACK, 1);
            if (lit_pixels > 0) {
                display_draw_line(dark_end, cy + y, right_edge, cy + y, 255, 1);
            }
        } else {
            // Left side lit: draw bright on left, dark on right
            int bright_end = left_edge + lit_pixels;
            display_draw_line(left_edge, cy + y, right_edge, cy + y, COLOR_BLACK, 1);
            if (lit_pixels > 0) {
                display_draw_line(left_edge, cy + y, bright_end, cy + y, 255, 1);
            }
        }
    }

    // Draw outline
    display_draw_circle(cx, cy, radius, -1, COLOR_BLACK);
}

void draw_sun_icon(int cx, int cy, int radius) {
    display_draw_circle(cx, cy, radius, 255, COLOR_BLACK);
    int ray_length = 3;
    for (int angle = 0; angle < 360; angle += 45) {
        float rad = angle * M_PI / 180;
        int x1 = cx + (int)((radius + 1) * cos(rad));
        int y1 = cy + (int)((radius + 1) * sin(rad));
        int x2 = cx + (int)((radius + ray_length) * cos(rad));
        int y2 = cy + (int)((radius + ray_length) * sin(rad));
        display_draw_line(x1, y1, x2, y2, COLOR_BLACK, 1);
    }
}

void render_tide_display(time64_t target_time, const char *message) {
    float current_height = calculate_tide_height(target_time);
    TideEvent next_high, next_low;
    find_next_high_low(target_time, &next_high, &next_low);

    float moon_phase = calculate_moon_phase(target_time);
    time64_t next_full_moon, next_new_moon;
    find_next_full_new_moon(target_time, &next_full_moon, &next_new_moon);

    int sunrise_h, sunrise_m, sunset_h, sunset_m;
    calculate_sunrise_sunset(target_time, &sunrise_h, &sunrise_m, &sunset_h, &sunset_m);

    char high_time_str[8], low_time_str[8];
    char sunrise_str[8], sunset_str[8];
    char full_moon_str[8], new_moon_str[8];

    struct tm *tm_high = utc_to_margate(next_high.time);
    snprintf(high_time_str, sizeof(high_time_str), "%02d:%02d", tm_high->tm_hour, tm_high->tm_min);

    struct tm *tm_low = utc_to_margate(next_low.time);
    snprintf(low_time_str, sizeof(low_time_str), "%02d:%02d", tm_low->tm_hour, tm_low->tm_min);

    snprintf(sunrise_str, sizeof(sunrise_str), "%02d:%02d", sunrise_h, sunrise_m);
    snprintf(sunset_str, sizeof(sunset_str), "%02d:%02d", sunset_h, sunset_m);

    struct tm *tm_full = utc_to_margate(next_full_moon);
    snprintf(full_moon_str, sizeof(full_moon_str), "%02d/%02d", tm_full->tm_mday, tm_full->tm_mon + 1);

    struct tm *tm_new = utc_to_margate(next_new_moon);
    snprintf(new_moon_str, sizeof(new_moon_str), "%02d/%02d", tm_new->tm_mday, tm_new->tm_mon + 1);

    // --- Moon section at top ---
    int moon_cx = 30;
    int moon_cy = 24;
    int moon_radius = 18;
    draw_moon_phase(moon_cx, moon_cy, moon_radius, moon_phase);

    int dates_x = moon_cx + moon_radius + 12;
    display_draw_circle(dates_x + 4, 14, 4, 255, COLOR_BLACK);
    display_draw_text(dates_x + 12, 20, full_moon_str, FONT_MEDIUM);
    display_draw_circle(dates_x + 4, 28, 4, COLOR_BLACK, COLOR_BLACK);
    display_draw_text(dates_x + 12, 34, new_moon_str, FONT_MEDIUM);

    // --- Sunrise/Sunset section ---
    int sun_icon_y = 54;
    int sun_times_y = 68;

    int sunrise_width = display_get_text_width(sunrise_str, FONT_MEDIUM);
    int sun_radius = 5;
    int sun_cx = DISPLAY_WIDTH / 2;
    draw_sun_icon(sun_cx, sun_icon_y, sun_radius);

    int times_gap = 10;
    display_draw_text(sun_cx - times_gap - sunrise_width, sun_times_y, sunrise_str, FONT_MEDIUM);
    display_draw_text(sun_cx + times_gap, sun_times_y, sunset_str, FONT_MEDIUM);

    int daylight_bar_y = sun_times_y + 6;
    int daylight_bar_h = 6;
    display_draw_rect(0, daylight_bar_y, DISPLAY_WIDTH, daylight_bar_h, 255, COLOR_BLACK);

    float sunrise_frac = (sunrise_h + sunrise_m / 60.0) / 24.0;
    float sunset_frac = (sunset_h + sunset_m / 60.0) / 24.0;
    int sunrise_x = (int)(sunrise_frac * DISPLAY_WIDTH);
    int sunset_x = (int)(sunset_frac * DISPLAY_WIDTH);
    display_draw_rect(1, daylight_bar_y + 1, sunrise_x - 2, daylight_bar_h - 2, COLOR_BLACK, 255);
    display_draw_rect(sunset_x + 1, daylight_bar_y + 1, DISPLAY_WIDTH - sunset_x - 2, daylight_bar_h - 2, COLOR_BLACK, 255);

    // --- Tide section ---
    int tide_bar_w = 15;
    int tide_bar_x = (DISPLAY_WIDTH - tide_bar_w) / 2;
    int tide_bar_y = 105;
    int tide_bar_h = 109;

    float tide_range = next_high.height - next_low.height;
    float normalized = 0.5;
    if (tide_range > 0) {
        normalized = (current_height - next_low.height) / tide_range;
        normalized = fmax(0, fmin(1, normalized));
    }

    int tide_rising = (next_high.time < next_low.time) ? 1 : 0;
    float current_phase = asin(fmax(-1, fmin(1, 2 * normalized - 1)));
    if (!tide_rising) {
        current_phase = M_PI - current_phase;
    }

    int high_time_width = display_get_text_width(high_time_str, FONT_MEDIUM);
    int high_time_x = tide_bar_x + (tide_bar_w - high_time_width) / 2;
    display_draw_text(high_time_x, tide_bar_y - 5, high_time_str, FONT_MEDIUM);

    // Draw sine wave
    int wave_y_center = tide_bar_y + tide_bar_h / 2;
    int wave_amplitude = (tide_bar_h - 2) / 2;
    float tidal_period = 12.42;
    float hours_to_show = 6.0;
    int prev_x = -1, prev_y = -1;

    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        float x_fraction = (float)(x - DISPLAY_WIDTH/2) / (DISPLAY_WIDTH/2);
        float hours_offset = x_fraction * (hours_to_show / 2);
        float phase_offset = (hours_offset / tidal_period) * 2 * M_PI;
        float phase = current_phase + phase_offset;
        float tide_level = sin(phase);
        int y = wave_y_center - (int)(tide_level * wave_amplitude);

        if (prev_x >= 0) {
            int in_bar = (x >= tide_bar_x && x < tide_bar_x + tide_bar_w);
            int prev_in_bar = (prev_x >= tide_bar_x && prev_x < tide_bar_x + tide_bar_w);
            if (!in_bar && !prev_in_bar) {
                display_draw_line(prev_x, prev_y, x, y, COLOR_BLACK, 2);
            }
        }
        prev_x = x;
        prev_y = y;
    }

    display_draw_rect(tide_bar_x, tide_bar_y, tide_bar_w, tide_bar_h, COLOR_WHITE, COLOR_BLACK);

    int fill_h = (int)(normalized * (tide_bar_h - 2));
    if (fill_h > 0) {
        display_draw_rect(tide_bar_x + 1, tide_bar_y + tide_bar_h - fill_h - 1,
                          tide_bar_w - 2, fill_h, COLOR_BLACK, 255);
    }

    int low_time_width = display_get_text_width(low_time_str, FONT_MEDIUM);
    int low_time_x = tide_bar_x + (tide_bar_w - low_time_width) / 2;
    display_draw_text(low_time_x, tide_bar_y + tide_bar_h + 15, low_time_str, FONT_MEDIUM);

    // --- Optional message ---
    if (message && strlen(message) > 0) {
        int msg_width = display_get_text_width(message, FONT_MEDIUM);
        int msg_x = (DISPLAY_WIDTH - msg_width) / 2;
        display_draw_text(msg_x, DISPLAY_HEIGHT - 46, message, FONT_MEDIUM);
    }

    // --- Location section ---
    int line_y = DISPLAY_HEIGHT - 33;
    display_draw_line(0, line_y, DISPLAY_WIDTH, line_y, COLOR_BLACK, 2);

    int margate_width = display_get_text_width("Margate", FONT_LARGE);
    int margate_x = (DISPLAY_WIDTH - margate_width) / 2;
    display_draw_text(margate_x, DISPLAY_HEIGHT - 8, "Margate", FONT_LARGE);
}

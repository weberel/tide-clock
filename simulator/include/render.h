#ifndef RENDER_H
#define RENDER_H

#include "timezone.h"

/**
 * Draw moon phase icon.
 *
 * @param cx Center X coordinate
 * @param cy Center Y coordinate
 * @param radius Moon radius in pixels
 * @param phase Phase 0-1 where 0=new, 0.5=full
 */
void draw_moon_phase(int cx, int cy, int radius, float phase);

/**
 * Draw sun icon with rays.
 *
 * @param cx Center X coordinate
 * @param cy Center Y coordinate
 * @param radius Sun radius in pixels
 */
void draw_sun_icon(int cx, int cy, int radius);

/**
 * Render complete tide display.
 * Draws moon, sun times, tide bar, and location.
 *
 * @param target_time Time to render for (UTC)
 * @param message Optional message to display (NULL for none)
 */
void render_tide_display(time64_t target_time, const char *message);

#endif // RENDER_H

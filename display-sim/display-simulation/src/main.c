/**
 * Tide Display - Main Entry Point
 *
 * Orchestrates display rendering for PC simulation.
 * STM32 version will add sleep/wake cycle and NFC time sync.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "display.h"
#include "tide.h"
#include "astro.h"
#include "render.h"
#include "timezone.h"

// Future STM32 includes:
// #include "ds3231.h"
// #include "nt3h2111.h"
// #include "power.h"

// ============================================================================
// Message System (matches firmware exactly)
// ============================================================================

// Special dates table - matches firmware special_dates[]
typedef struct {
    int month;
    int day;
    const char *message;
} SpecialDate;

static const SpecialDate special_dates[] = {
    // Personal
    // Holidays
    {1, 1, "Happy New Year!"},
    {2, 14, "Happy Valentine's!"},
    {10, 31, "Happy Halloween!"},
    {12, 24, "Merry Christmas!"},
    {12, 31, "Happy New Year!"},
    // Seasons
    {3, 1, "Spring is here!"},
    {6, 1, "Hello Summer"},
    {9, 1, "Autumn begins"},
    {12, 1, "Winter is here"},
    // Fun
    {3, 14, "3.1415926535897.."},
    // Meteor showers (peak nights)
    {1, 3, "Quadrantids tonight"},
    {1, 4, "Quadrantids tonight"},
    {4, 22, "Lyrids tonight"},
    {8, 11, "Perseids tonight"},
    {8, 12, "Perseids tonight"},
    {8, 13, "Perseids tonight"},
    {10, 21, "Orionids tonight"},
    {11, 17, "Leonids tonight"},
    {12, 13, "Geminids tonight"},
    {12, 14, "Geminids tonight"},
    {12, 15, "Geminids tonight"},
    {0, 0, NULL}  // End marker
};

// May 1st rotating messages - cycles each year
static const char* may_day_messages[] = {
    "Power to the people",
    "Solidarity forever",
    "Workers unite",
    "Seize the means",
    "Organize, agitate",
    "Unite and fight",
    "International unity",
    NULL  // End marker
};

// Eclipse dates visible from UK/Margate (2025-2030 for simulator)
typedef struct {
    int date;  // YYYYMMDD format
    const char *message;
} EclipseDate;

static const EclipseDate eclipse_dates[] = {
    // 2020s
    {20251207, "Lunar Eclipse"},
    {20260812, "Solar Eclipse"},
    {20281231, "Lunar Eclipse"},
    {20290626, "Lunar Eclipse"},
    {20291220, "Lunar Eclipse"},
    // 2030s
    {20321018, "Lunar Eclipse"},
    {20330414, "Lunar Eclipse"},
    {0, NULL}  // End marker
};

// Calculate equinox/solstice dates for a given year
// Uses Meeus algorithm - accurate to within a few minutes
// event: 0=Spring Equinox, 1=Summer Solstice, 2=Autumn Equinox, 3=Winter Solstice
static void get_solstice_equinox(int year, int event, int *month, int *day) {
    double Y = (year - 2000) / 1000.0;
    double JDE0;

    switch (event) {
        case 0:  // Spring Equinox (March)
            JDE0 = 2451623.80984 + 365242.37404 * Y + 0.05169 * Y * Y
                   - 0.00411 * Y * Y * Y - 0.00057 * Y * Y * Y * Y;
            break;
        case 1:  // Summer Solstice (June)
            JDE0 = 2451716.56767 + 365241.62603 * Y + 0.00325 * Y * Y
                   + 0.00888 * Y * Y * Y - 0.00030 * Y * Y * Y * Y;
            break;
        case 2:  // Autumn Equinox (September)
            JDE0 = 2451810.21715 + 365242.01767 * Y - 0.11575 * Y * Y
                   + 0.00337 * Y * Y * Y + 0.00078 * Y * Y * Y * Y;
            break;
        case 3:  // Winter Solstice (December)
        default:
            JDE0 = 2451900.05952 + 365242.74049 * Y - 0.06223 * Y * Y
                   - 0.00823 * Y * Y * Y + 0.00032 * Y * Y * Y * Y;
            break;
    }

    // Convert Julian Day to calendar date
    int Z = (int)(JDE0 + 0.5);
    int A;
    if (Z < 2299161) {
        A = Z;
    } else {
        int alpha = (int)((Z - 1867216.25) / 36524.25);
        A = Z + 1 + alpha - alpha / 4;
    }
    int B = A + 1524;
    int C = (int)((B - 122.1) / 365.25);
    int D = (int)(365.25 * C);
    int E = (int)((B - D) / 30.6001);

    *day = B - D - (int)(30.6001 * E);
    if (E < 14) {
        *month = E - 1;
    } else {
        *month = E - 13;
    }
}

// Check if date matches an eclipse
static const char* get_eclipse_message(int year, int month, int day) {
    int date = year * 10000 + month * 100 + day;
    for (int i = 0; eclipse_dates[i].message != NULL; i++) {
        if (eclipse_dates[i].date == date) {
            return eclipse_dates[i].message;
        }
    }
    return NULL;
}

// Check if date is a solstice or equinox
static const char* get_solar_event_message(int year, int month, int day) {
    int event_month, event_day;

    // Check Spring Equinox
    get_solstice_equinox(year, 0, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Spring Equinox";
    }

    // Check Summer Solstice
    get_solstice_equinox(year, 1, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Summer Solstice";
    }

    // Check Autumn Equinox
    get_solstice_equinox(year, 2, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Autumn Equinox";
    }

    // Check Winter Solstice
    get_solstice_equinox(year, 3, &event_month, &event_day);
    if (month == event_month && day == event_day) {
        return "Winter Solstice";
    }

    return NULL;
}

// Get special message for a date (matches firmware priority)
const char* get_message_for_date_full(int year, int month, int day) {
    // May 1st - rotate through messages based on year
    if (month == 5 && day == 1) {
        int count = 0;
        while (may_day_messages[count] != NULL) count++;
        return may_day_messages[year % count];
    }

    // Check for eclipse (high priority)
    const char* eclipse_msg = get_eclipse_message(year, month, day);
    if (eclipse_msg != NULL) {
        return eclipse_msg;
    }

    // Check for solstice/equinox
    const char* solar_msg = get_solar_event_message(year, month, day);
    if (solar_msg != NULL) {
        return solar_msg;
    }

    // Regular lookup
    for (int i = 0; special_dates[i].message != NULL; i++) {
        if (special_dates[i].month == month && special_dates[i].day == day) {
            return special_dates[i].message;
        }
    }
    return NULL;
}

// ============================================================================
// Animation Generation (PC only)
// ============================================================================

void generate_frames(const char *output_dir, time64_t start_time, int num_frames, int hours_per_frame) {
    char filename[256];
    char cmd[512];

    snprintf(cmd, sizeof(cmd), "mkdir -p %s", output_dir);
    system(cmd);

    for (int i = 0; i < num_frames; i++) {
        time64_t frame_time = start_time + (time64_t)(i * hours_per_frame * 3600);
        struct tm *tm_info = utc_to_margate(frame_time);

        const char *msg = get_message_for_date_full(tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);

        display_clear();
        render_tide_display(frame_time, msg);

        snprintf(filename, sizeof(filename), "%s/frame_%04d.png", output_dir, i);
        display_update(filename);

        if (i % 10 == 0) {
            printf("Generated frame %d/%d (%04d-%02d-%02d)\n", i + 1, num_frames,
                   tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
        }
    }
}

// ============================================================================
// Startup Screens (for manual/documentation)
// ============================================================================

void render_screen_timeset(void) {
    // "SET TIME?" prompt screen
    display_clear();
    display_draw_text(5, 40, "SET TIME?", FONT_MEDIUM);
    display_draw_text(5, 65, "Hold button 2s", FONT_SMALL);
    display_draw_text(5, 90, "Wait 5s to skip", FONT_SMALL);
}

void render_screen_timeset_entry(int current_digit) {
    // Time entry screen with digits
    int digits[12] = {2, 0, 2, 5, 0, 1, 1, 5, 1, 2, 3, 0};  // 2025-01-15 12:30
    char buf[32];

    display_clear();
    display_draw_text(5, 14, "SET TIME", FONT_MEDIUM);
    display_draw_text(5, 28, "Enter digits. LED=ready.", FONT_SMALL);

    // Date line
    snprintf(buf, sizeof(buf), "%d%d%d%d-%d%d-%d%d",
             digits[0], digits[1], digits[2], digits[3],
             digits[4], digits[5], digits[6], digits[7]);
    display_draw_text(5, 50, buf, FONT_MEDIUM);

    // Time line
    snprintf(buf, sizeof(buf), "%d%d:%d%d",
             digits[8], digits[9], digits[10], digits[11]);
    display_draw_text(5, 75, buf, FONT_MEDIUM);

    // Draw underline under current digit
    int underline_x = 5;
    int underline_y = 52;
    int char_width = 8;  // Approximate

    if (current_digit < 4) {
        underline_x = 5 + current_digit * char_width;
    } else if (current_digit < 6) {
        underline_x = 5 + 4 * char_width + 6 + (current_digit - 4) * char_width;  // After "YYYY-"
    } else if (current_digit < 8) {
        underline_x = 5 + 4 * char_width + 6 + 2 * char_width + 6 + (current_digit - 6) * char_width;  // After "YYYY-MM-"
    } else {
        underline_y = 77;
        if (current_digit < 10) {
            underline_x = 5 + (current_digit - 8) * char_width;
        } else {
            underline_x = 5 + 2 * char_width + 6 + (current_digit - 10) * char_width;  // After "HH:"
        }
    }
    display_draw_line(underline_x, underline_y, underline_x + 6, underline_y, COLOR_BLACK, 2);

    // Instructions
    display_draw_text(5, 95, "Press=+1, Wait 2s=next", FONT_SMALL);
    display_draw_text(5, 108, "Hold 2s=0+next", FONT_SMALL);
    display_draw_text(5, 121, "Hold 4s=redo prev", FONT_SMALL);
}

void render_screen_boot_info(time64_t t) {
    // Boot info screen showing date/time and battery
    struct tm *local = utc_to_margate(t);
    char date_str[32], time_str[16], batt_str[16];

    snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d",
             local->tm_mday, local->tm_mon + 1, local->tm_year + 1900);
    snprintf(time_str, sizeof(time_str), "%02d:%02d",
             local->tm_hour, local->tm_min);
    snprintf(batt_str, sizeof(batt_str), "2.95V  79%%");  // Example values

    display_clear();

    int date_w = display_get_text_width(date_str, FONT_LARGE);
    int time_w = display_get_text_width(time_str, FONT_LARGE);
    int batt_w = display_get_text_width(batt_str, FONT_MEDIUM);

    display_draw_text((DISPLAY_WIDTH - date_w) / 2, 45, date_str, FONT_LARGE);
    display_draw_text((DISPLAY_WIDTH - time_w) / 2, 75, time_str, FONT_LARGE);
    display_draw_text((DISPLAY_WIDTH - batt_w) / 2, 105, batt_str, FONT_MEDIUM);
}

void render_screen_time_confirmed(time64_t t) {
    // Time set confirmation screen
    struct tm *local = utc_to_margate(t);
    char buf[32];

    display_clear();
    display_draw_text(5, 30, "TIME SET!", FONT_MEDIUM);

    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
    display_draw_text(5, 55, buf, FONT_MEDIUM);

    snprintf(buf, sizeof(buf), "%02d:%02d", local->tm_hour, local->tm_min);
    display_draw_text(5, 80, buf, FONT_MEDIUM);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    int rotation = ROTATION_0;
    time64_t target_time = (time64_t)time(NULL);
    const char *message = NULL;
    int generate_monthly = 0;
    int generate_yearly = 0;
    int fontset = FONTSET_HELVETICA;
    const char *screen = NULL;
    int timeset_digit = 0;

    // Parse command line
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "90") == 0) rotation = ROTATION_90;
        else if (strcmp(argv[i], "180") == 0) rotation = ROTATION_180;
        else if (strcmp(argv[i], "270") == 0) rotation = ROTATION_270;
        else if (strcmp(argv[i], "--monthly") == 0) generate_monthly = 1;
        else if (strcmp(argv[i], "--yearly") == 0) generate_yearly = 1;
        else if (strncmp(argv[i], "--date=", 7) == 0) {
            int year, month, day, hour = 12, minute = 0;
            if (sscanf(argv[i] + 7, "%d-%d-%d_%d:%d",
                       &year, &month, &day, &hour, &minute) >= 3) {
                target_time = parse_margate_time(year, month, day, hour, minute);
            }
        }
        else if (strncmp(argv[i], "--message=", 10) == 0) {
            message = argv[i] + 10;
        }
        else if (strncmp(argv[i], "--font=", 7) == 0) {
            const char *fname = argv[i] + 7;
            if (strcmp(fname, "helvetica") == 0) fontset = FONTSET_HELVETICA;
            else if (strcmp(fname, "profont") == 0) fontset = FONTSET_PROFONT;
            else if (strcmp(fname, "ncenr") == 0) fontset = FONTSET_NCENR;
            else if (strcmp(fname, "courr") == 0) fontset = FONTSET_COURR;
            else if (strcmp(fname, "haxr") == 0) fontset = FONTSET_HAXR;
            else if (strcmp(fname, "pixelle") == 0) fontset = FONTSET_PIXELLE;
        }
        else if (strncmp(argv[i], "--screen=", 9) == 0) {
            screen = argv[i] + 9;
        }
        else if (strncmp(argv[i], "--digit=", 8) == 0) {
            timeset_digit = atoi(argv[i] + 8);
        }
    }

    display_set_fontset(fontset);
    display_init_with_rotation(rotation);

    // Handle startup screen rendering
    if (screen) {
        char output_file[64];
        snprintf(output_file, sizeof(output_file), "screen_%s.png", screen);

        if (strcmp(screen, "timeset") == 0) {
            render_screen_timeset();
        } else if (strcmp(screen, "timeset_entry") == 0) {
            render_screen_timeset_entry(timeset_digit);
        } else if (strcmp(screen, "boot") == 0) {
            render_screen_boot_info(target_time);
        } else if (strcmp(screen, "confirmed") == 0) {
            render_screen_time_confirmed(target_time);
        } else {
            printf("Unknown screen: %s\n", screen);
            printf("Available screens: timeset, timeset_entry, boot, confirmed\n");
            return 1;
        }

        display_update(output_file);
        printf("Saved: %s\n", output_file);
    }
    else if (generate_monthly) {
        time64_t start = parse_margate_time(2025, 1, 1, 12, 0);

        printf("Generating monthly animation (January 2025)...\n");
        generate_frames("frames_monthly", start, 120, 6);

        printf("\nCreating GIF...\n");
        system("ffmpeg -y -framerate 10 -i frames_monthly/frame_%04d.png -vf \"split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" tide_monthly.gif 2>/dev/null");
        printf("Saved: tide_monthly.gif\n");
    }
    else if (generate_yearly) {
        time64_t start = parse_margate_time(2025, 1, 1, 12, 0);

        printf("Generating yearly animation (2025)...\n");
        generate_frames("frames_yearly", start, 365, 24);

        printf("\nCreating GIF...\n");
        system("ffmpeg -y -framerate 20 -i frames_yearly/frame_%04d.png -vf \"split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" tide_yearly.gif 2>/dev/null");
        printf("Saved: tide_yearly.gif\n");
    }
    else {
        struct tm *tm_info = utc_to_margate(target_time);
        int bst = is_bst(target_time);

        if (!message) {
            message = get_message_for_date_full(tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
        }

        printf("Tide Display for: %04d-%02d-%02d %02d:%02d %s (Margate time)\n",
               tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
               tm_info->tm_hour, tm_info->tm_min, bst ? "BST" : "GMT");
        printf("Display: %dx%d (rotation=%d)\n", DISPLAY_WIDTH, DISPLAY_HEIGHT, rotation * 90);
        if (message) printf("Message: %s\n", message);

        // Calculate and print all display values for verification
        TideEvent next_high, next_low;
        find_next_high_low(target_time, &next_high, &next_low);
        struct tm *tm_high = utc_to_margate(next_high.time);
        int hw_hour = tm_high->tm_hour, hw_min = tm_high->tm_min;
        struct tm *tm_low = utc_to_margate(next_low.time);
        int lw_hour = tm_low->tm_hour, lw_min = tm_low->tm_min;
        printf("Next HW: %02d:%02d (local)\n", hw_hour, hw_min);
        printf("Next LW: %02d:%02d (local)\n", lw_hour, lw_min);

        int sunrise_h, sunrise_m, sunset_h, sunset_m;
        calculate_sunrise_sunset(target_time, &sunrise_h, &sunrise_m, &sunset_h, &sunset_m);
        printf("Sunrise: %02d:%02d | Sunset: %02d:%02d\n", sunrise_h, sunrise_m, sunset_h, sunset_m);

        float moon_phase = calculate_moon_phase(target_time);
        printf("Moon phase: %.3f (0=new, 0.5=full)\n", moon_phase);

        display_clear();
        render_tide_display(target_time, message);
        display_update("tide_display.png");

        printf("Saved: tide_display.png\n");
    }

    return 0;
}

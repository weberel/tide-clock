# Tide Display - C Implementation

E-paper tide display for Margate, UK. Generates PNG images matching the physical 128x296 pixel display.

## File Structure

```
display-sim/display-simulation/
├── src/
│   ├── main.c               # Entry point, CLI, orchestration
│   ├── tide.c               # Harmonic prediction (31 constituents)
│   ├── astro.c              # Moon phase (Meeus), sunrise/sunset (NOAA)
│   ├── render.c             # Display drawing functions
│   ├── timezone.c           # BST/GMT handling
│   ├── display_u8g2_sdl.c   # SDL display backend
│   └── u8x8_d_sdl_128x296.c # u8g2 driver for 128x296 display
├── include/
│   ├── tide.h               # Tide prediction API
│   ├── astro.h              # Astronomical calculations API
│   ├── render.h             # Rendering API
│   ├── timezone.h           # Timezone handling API
│   ├── ds3231.h             # RTC driver (STM32, stubs ready)
│   ├── button.h             # Button input for time setting
│   ├── power.h              # Sleep/wake management
│   ├── display.h            # Display abstraction API
│   ├── u8g2_sdl.h           # SDL integration header
│   └── stb_image_write.h    # PNG output (single-header library)
├── u8g2/                    # u8g2 graphics library
├── pinout.txt               # STM32F411 pin assignments
└── Makefile
```

## Building

```bash
# Install SDL2 (required)
sudo apt install libsdl2-dev

# Build
make

# Run
./tide_sim                           # Current time
./tide_sim --date=2025-06-21_12:00   # Specific date (Margate local time)
./tide_sim --monthly                 # Generate month animation
./tide_sim --font=profont            # Different font (helvetica, profont, ncenr, courr, haxr, pixelle)
```

## Module Overview

### Tide Prediction (tide.c)

| Function | Description | Accuracy |
|----------|-------------|----------|
| `calculate_tide_height(time_t)` | Harmonic prediction (31 constituents) | Height: ±10cm |
| `find_next_high_low(time_t, ...)` | Find next HW/LW with correction | Time: MAE 4.2 min vs PLA |
| `get_time_correction(year, month, day)` | Nodal + annual + semi-annual correction | Reduces MAE by ~16% |

**Tide Model:**
- 31 harmonic constituents fitted via least-squares to PLA minute data (2019-2026)
- Epoch: 2019-01-01 00:09:00 UTC
- Mean sea level: 2.64m (Chart Datum)
- Empirical correction for 18.61-year nodal cycle + seasonal effects

### Moon Phase (astro.c - Meeus Algorithm)

| Function | Description | Accuracy |
|----------|-------------|----------|
| `calculate_moon_phase(time_t)` | Current illumination (0=new, 0.5=full) | Correct date |
| `find_next_full_new_moon(time_t, ...)` | Next full & new moon times | ±3 min (2020-2040) |

**Algorithm:** Jean Meeus "Astronomical Algorithms" Chapter 49
- Uses lunation number k (k=0 is Jan 6, 2000 new moon)
- Includes 15 perturbation correction terms

### Sunrise/Sunset (astro.c - NOAA Algorithm)

| Function | Description | Accuracy |
|----------|-------------|----------|
| `calculate_sunrise_sunset(time_t, ...)` | Sunrise/sunset times (local) | ±5 min |

**Algorithm:** NOAA Solar Calculator
- Uses equation of time and solar declination
- Accounts for atmospheric refraction (zenith 90.833°)
- Automatically adds BST offset in summer

### Timezone Handling (timezone.c)

| Function | Description |
|----------|-------------|
| `is_bst(time_t)` | Check if UTC time is during BST |
| `utc_to_margate(time_t)` | Convert UTC to Margate local time |
| `parse_margate_time(...)` | Parse local time input to UTC |

**BST Rules:**
- Starts: 01:00 UTC on last Sunday of March
- Ends: 01:00 UTC on last Sunday of October

### Display Rendering (render.c)

| Function | Description |
|----------|-------------|
| `render_tide_display(time_t, message)` | Main render function |
| `draw_moon_phase(cx, cy, r, phase)` | Moon with shadow |
| `draw_sun_icon(cx, cy, r)` | Sun with rays |

## Accuracy Summary

| Component | Range | Mean Error | Max Error |
|-----------|-------|------------|-----------|
| Tide times | 2010-2040 | 4.2 min | ~15 min |
| Tide heights | 2019-2026 | ~10 cm | ~30 cm |
| Moon phases | 2020-2040 | 1.7 min | 3.1 min |
| Sunrise/sunset | 2025-2040 | 2 min | 5 min |

## STM32F411 Hardware

### Pin Assignments (see pinout.txt)

| Function | Pin | Notes |
|----------|-----|-------|
| Wakeup (RTC) | PA0 | WKUP1, via P-FET inverter |
| DS3231 VCC gate | PA1 | P-FET, needs pulldown for boot |
| Button input | PA2 | Active LOW, for time setting |
| Display VCC gate | PA3 | P-FET, needs pulldown for boot |
| Display SPI | PA4-PA7 | CS, CLK, DC, DIN |
| Display control | PA8-PA9 | RST, Busy |
| I2C1 | PB6-PB7 | SCL, SDA (DS3231) |
| DS3231 32kHz out | PC14 | Optional LSE input |

### Power Management

The device uses P-channel MOSFETs to gate power to peripherals:
- **DS3231**: Always on (VBAT backup), VCC gated for I2C communication
- **Display**: Powered during refresh only (~3 seconds)

### Wakeup Sources

1. **RTC Alarm**: DS3231 INT pin (active-low) → P-FET inverter → PA0 rising edge
2. **Button**: PA2 button press can wake from sleep for time setting

## STM32 Porting Notes

Code is portable to STM32F411:
- Uses `float` (single precision) throughout for Cortex-M4 FPU
- Pre-computed angular speeds in radians for efficiency
- Only stdlib dependencies: `<math.h>`, `<time.h>`, `<string.h>`

To port:
1. Implement `ds3231_i2c_read/write()` in ds3231_platform.c
2. Implement `power_platform_shutdown()` using STM32LowPower
3. Replace display backend with e-paper SPI driver

# Tide Display Simulator

E-paper tide display for Margate, UK - a gift project simulating a 128x296 pixel monochrome display showing tide times, moon phases, and sunrise/sunset.

## Project Overview

This project creates a tide clock display that shows:
- Current tide state (rising/falling) with animated indicator
- Next high and low tide times
- Moon phase with next full/new moon dates
- Sunrise and sunset times
- Location name (Margate)
- Optional date-specific messages from CSV

## Directory Structure

```
display-sim/
├── c/                          # C implementation (production)
│   ├── src/
│   │   ├── main.c              # Main application, tide calculations, rendering
│   │   ├── display_u8g2_sdl.c  # SDL display driver for simulation
│   │   └── u8x8_d_sdl_128x296.c # Custom u8g2 driver for 128x296 display
│   ├── include/                # Header files
│   ├── u8g2/                   # u8g2 graphics library (submodule)
│   ├── messages.csv            # Date-specific messages
│   └── Makefile
│
├── python/                     # Python implementation (prototype)
│   ├── epaper_sim.py           # Original Python simulator
│   ├── simulate_date.py        # Date simulation tool
│   └── messages.csv
│
└── harmonic_fitting/           # Tidal harmonic analysis
    ├── fit_harmonics.py        # Least-squares fitting to tide data
    ├── margate_ticon.py        # TICON-based tide prediction
    ├── margate_harmonics.json  # Fitted harmonic constants
    ├── test_uptide.py          # Testing with uptide library
    └── TICON/                  # Professional tide gauge data
        ├── TICON.txt           # 1145 stations, 40 constituents each
        └── TICON_userManual.pdf
```

## Building and Running

### C Version (Recommended)
```bash
cd c
make clean && make
./tide_sim              # Interactive SDL window
./tide_sim --headless   # Generate tide_display.png only
```

### Python Version
```bash
cd python
python3 epaper_sim.py
```

## Tide Calculation

### Harmonic Model
The tide prediction uses 8 main harmonic constituents derived from the TICON dataset (GESLA/BODC):

| Constituent | Description | Period (h) | Amplitude (m) |
|-------------|-------------|------------|---------------|
| M2 | Principal lunar semidiurnal | 12.42 | 2.15 |
| S2 | Principal solar semidiurnal | 12.00 | 0.66 |
| N2 | Larger lunar elliptic | 12.66 | 0.38 |
| K2 | Lunisolar semidiurnal | 11.97 | 0.19 |
| K1 | Lunisolar diurnal | 23.93 | 0.07 |
| O1 | Lunar diurnal | 25.82 | 0.08 |
| P1 | Solar diurnal | 24.07 | 0.03 |
| Q1 | Larger lunar elliptic diurnal | 26.87 | 0.03 |

Note: Shallow water harmonics (M4, MS4) were tested but removed as they increased prediction error.

**Accuracy**: ~20 minutes average time error, ~0.2m height error compared to official tide tables.

### Data Source
Constants interpolated from:
- Dover (51.12°N, 1.32°E) - BODC tide gauge, 90 years of data (65% weight)
- Sheerness (51.44°N, 0.74°E) - BODC tide gauge, 58 years of data (35% weight)

Margate (51.38°N, 1.39°E) lies between these stations. Amplitudes scaled by 0.99.

### Formula
```c
height = mean_sea_level;
for each constituent:
    angle = speed * hours_since_1900 - phase;  // degrees
    height += amplitude * cos(angle * PI / 180);
```

## Display Layout (128x296 pixels)

```
┌────────────────────┐
│  ○ 05/12           │  Moon phase + full moon date
│  ● 20/12           │  New moon date
├────────────────────┤
│ 07:30 ☀ 15:53      │  Sunrise/sunset times
├────────────────────┤
│      ┌─────┐       │
│      │05:05│       │  Next high tide time (box)
│      └─────┘       │
│         │          │
│         █          │  Tide bar (animated)
│         │          │
│      23:05         │  Next low tide time
├────────────────────┤
│     Margate        │  Location name
└────────────────────┘
```

## Key Files

### `c/src/main.c`
- `calculate_tide_height(time_t dt)` - Returns tide height in meters
- `find_next_high_low()` - Finds next high/low tide times
- `render_tide_display()` - Main rendering function
- `calculate_moon_phase()` - Moon illumination calculation
- `calculate_sunrise_sunset()` - Solar position calculation

### `harmonic_fitting/margate_ticon.py`
- Reference Python implementation with same algorithm as C code
- Used for validation and testing new harmonic constants

### `harmonic_fitting/TICON/`
- Professional tidal constants from GESLA dataset
- 40 constituents for 1145 tide gauges worldwide
- Source: British Oceanographic Data Centre (BODC)

## Configuration

### Location (Margate, UK)
```c
#define LATITUDE 51.3891
#define LONGITUDE 1.3869
#define MEAN_SEA_LEVEL 2.76  // meters above chart datum
```

### Timezone
Display shows GMT/UTC (Margate local time). Code uses `gmtime()` and `timegm()` for all time calculations regardless of system timezone.

### Custom Messages
Edit `messages.csv` to add date-specific messages:
```csv
date,message
25-12,Merry Christmas!
01-01,Happy New Year!
```

## Dependencies

### C Version
- SDL2 (`libsdl2-dev`)
- u8g2 graphics library (included as submodule)
- Standard C math library

### Python Version
- PIL/Pillow
- numpy
- scipy (for harmonic fitting)
- uptide (for professional tide calculations)

## Hardware Target

Designed for deployment on STM32 microcontroller with:
- 2.9" e-paper display (296x128 pixels, rotated 90°)
- SPI interface
- Battery powered with deep sleep
- RTC for timekeeping

The SDL simulator matches the exact pixel layout and font rendering of the target hardware.

# Simulator

SDL2-based simulator for testing the display without hardware.

## Building

```bash
# Prerequisites (Ubuntu/Debian)
sudo apt install libsdl2-dev gcc make

# Clone u8g2 and build
git clone https://github.com/olikraus/u8g2.git
make

# Run (opens SDL window)
./tide_sim

# Headless mode (generates PNG)
./tide_sim --headless

# Specific date (local time)
./tide_sim --date=2025-06-21_12:00
```

## Configuration

Uses the same `include/location_config.h` as firmware. Generate with:

```bash
cd scripts
python3 generate_location.py --noaa 9414290 --name "San Francisco" --tz US_PACIFIC
```

## Key Files

| File | Description |
|------|-------------|
| `src/main.c` | SDL setup, event loop |
| `src/tide.c` | Harmonic tide prediction |
| `src/astro.c` | Moon phase, sunrise/sunset |
| `src/timezone.c` | DST handling |
| `src/render.c` | Display layout |
| `include/location_config.h` | Location parameters (generated) |

The core source files are identical to firmware.

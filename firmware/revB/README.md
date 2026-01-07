# Tide Display - STM32 Hardware (Rev B)

E-paper tide display for Margate, UK using STM32F411CE and 2.9" SSD1680 tri-color display.

**This is the Rev B firmware.** See [firmware-revA](../firmware-revA/) for Rev A hardware.

## Hardware Changes (Rev B)

| Feature | Rev A | Rev B |
|---------|-------|-------|
| NFC chip | NT3H2111 (unused) | ST25DV04K (for time sync) |
| Time setting | Button on PB1 | NFC (phone app) |
| PA2 function | NT3H2111 power gate | NFC LPD control |
| Button | PB1 (active LOW) | Removed |
| Wakeup | RTC alarm only | RTC alarm OR NFC field |
| External power | None | PB9 (3V3 gated output) |

## Features

- Current tide height with visual bar indicator
- Next high/low tide times (31-constituent harmonic model, ~5 min accuracy)
- Sunrise/sunset times with daylight bar
- Moon phase with next full/new moon dates
- Automatic BST/GMT timezone handling for UK
- 15-minute update cycle with deep sleep (~µA standby current)
- NFC time synchronization (planned)

## Operation

The device wakes from two sources:
1. **RTC alarm** (every 15 minutes) - normal display update
2. **NFC field** (phone tap) - time synchronization

Wake cycle:
1. PA0 rising edge wakes MCU from STOP mode
2. Determine wake source (check DS3231 alarm flag vs ST25DV field detect)
3. If NFC: read time sync data, update RTC if valid
4. If RTC alarm: update display
5. Set next alarm, enter STOP mode

## Building & Uploading

Requires PlatformIO.

```bash
# Build
pio run

# Upload via ST-Link
pio run --target upload
```

**Note:** If upload fails due to device in sleep mode, hold RESET while plugging in ST-Link, then release when upload starts.

## RTC Time Setting (Rev B)

Time is set via NFC:
1. Phone app writes time data to ST25DV user memory
2. Tapping phone to device wakes MCU via GPO → PA0
3. MCU reads time data, validates checksum
4. Updates DS3231 RTC
5. Display confirms sync (TODO)

See [kicad/versions.md](../../kicad/versions.md) for NFC time sync protocol details.

## Project Structure

```
firmware-revB/
├── src/
│   ├── main.c              # Entry point, RTC, NFC, sleep/wake cycle
│   ├── st25dv.c            # ST25DV04K NFC driver
│   ├── display_u8g2_stm32.c # Display HAL backend (SPI, GPIO)
│   ├── render.c            # UI rendering (layout, icons)
│   ├── tide.c              # Tide calculations (31-harmonic model)
│   ├── astro.c             # Sun/moon calculations (Meeus/NOAA)
│   └── timezone.c          # UK timezone handling (GMT/BST)
├── include/
│   ├── st25dv.h            # ST25DV04K driver header
│   └── *.h                 # Other headers
├── lib/
│   └── u8g2/               # u8g2 graphics library + custom SSD1680 driver
└── platformio.ini
```

## Pin Configuration (Rev B)

| Pin | Function |
|-----|----------|
| PA0 | Wakeup (OR'd: RTC alarm + NFC field) |
| PA1 | DS3231 VCC gate (P-FET, active LOW) |
| PA2 | ST25DV LPD (HIGH = low power) |
| PA3 | Display VCC gate (P-FET, active LOW) |
| PB3 | Debug output (SWO) |
| PB6/PB7 | I2C1 (DS3231 + ST25DV) |
| PB9 | External 3V3 gate (P-FET, active LOW) |
| PC11 | LED2 |
| PD2 | LED1 |

## TODO

- [ ] Implement NFC time sync in main loop
- [ ] Add display confirmation after time sync
- [ ] Test wakeup from NFC field detection
- [ ] Implement external power control (PB9)

---

# Rendering System Report

## Overview

The rendering system produces a 128x296 pixel display showing tide, sun, and moon information for Margate, UK. The display is rotated 90 degrees for a portrait orientation.

## Display Layout

```
┌─────────────────────────────────────┐
│  ┌───┐                              │
│  │   │ Moon    ○ 15/01  (full)      │  Moon Section (y: 0-50)
│  │   │ Phase   ● 01/01  (new)       │
│  └───┘                              │
├─────────────────────────────────────┤
│           ☀ Sun Icon                │  Sun Section (y: 50-80)
│       06:45        18:30            │  Sunrise/Sunset times
│ ██████░░░░░░░░░░░░░░░░░░░░██████   │  Daylight bar
├─────────────────────────────────────┤
│                                     │
│  ~~~~      ┌───┐      ~~~~          │
│       ~~~~ │███│ ~~~~               │  Tide Section (y: 100-220)
│            │███│                    │  Sine wave + fill bar
│       ____ │   │ ____               │
│  ____      └───┘      ____          │
│            14:30                    │  Next high/low time
│                                     │
├─────────────────────────────────────┤
│ ═══════════════════════════════════ │  Separator line
│            MARGATE                  │  Location (y: 260-296)
└─────────────────────────────────────┘
```

## Rendering Pipeline

### 1. Data Collection

```c
// tide.c - 31-constituent harmonic model
float current_height = calculate_tide_height(target_time);
find_next_high_low(target_time, &next_high, &next_low);

// astro.c - Meeus/NOAA algorithms
float moon_phase = calculate_moon_phase(target_time);
find_next_full_new_moon(target_time, &next_full_moon, &next_new_moon);
calculate_sunrise_sunset(target_time, &sunrise_h, &sunrise_m, &sunset_h, &sunset_m);
```

### 2. Time Conversion

All internal calculations use UTC. Display times are converted to local Margate time (GMT/BST):

```c
// timezone.c - handles EU timezone rules
struct tm *local = utc_to_margate(utc_time);
int is_daylight = is_bst(utc_time);
```

### 3. UI Rendering (render.c)

#### Moon Phase Icon

- **Size:** 36px diameter (radius=18)
- **Position:** Top-left (cx=30, cy=24)
- **Algorithm:** Draws white circle, then overlays black shadow using terminator calculation

```c
// Waxing (phase 0-0.5): shadow on LEFT, terminator moves right
// Waning (phase 0.5-1): shadow on RIGHT, terminator moves left
for (int y = -radius; y <= radius; y++) {
    int x_extent = sqrt(radius² - y²);
    int terminator_x = cx + terminator * x_extent;
    // Draw shadow line from edge to terminator
}
```

#### Sun Icon

- **Size:** 10px diameter with 3px rays
- **Position:** Center-top (cx=64, cy=54)
- **8 rays** at 45-degree intervals

#### Daylight Bar

- **Full width:** 128px
- **Shows:** Night (black) / Day (white) proportions
- **Calculation:** `sunrise_frac = (hour + min/60) / 24`

#### Tide Display

- **Central bar:** 15x109px vertical fill gauge
- **Fill level:** Normalized between next low and high tide heights
- **Sine wave:** 6-hour window centered on current time

```c
// Phase calculation for sine wave
float tidal_period = 12.42;  // hours (M2 constituent)
float current_phase = asin(2 * normalized - 1);
if (!tide_rising) current_phase = PI - current_phase;

// Draw wave, skip over central bar region
for (int x = 0; x < DISPLAY_WIDTH; x++) {
    float phase = current_phase + (hours_offset / tidal_period) * 2*PI;
    int y = wave_center - sin(phase) * amplitude;
}
```

## Tide Calculation Model

### Harmonic Constituents (31 total)

| Constituent | Amplitude (m) | Period (hours) | Description |
|-------------|---------------|----------------|-------------|
| M2          | 1.6236        | 12.42          | Principal lunar |
| S2          | 0.4758        | 12.00          | Principal solar |
| N2          | 0.2846        | 12.66          | Larger lunar elliptic |
| K2          | 0.1624        | 11.97          | Lunisolar declinational |
| K1          | 0.1031        | 23.93          | Lunisolar diurnal |
| O1          | 0.1272        | 25.82          | Principal lunar diurnal |
| M4          | 0.0525        | 6.21           | Shallow water overtide |
| ... | ... | ... | (24 more) |

**Mean Sea Level:** 2.64m (Chart Datum)

### Empirical Time Correction

Accounts for nodal cycle (18.61 years) and seasonal variations:

```c
correction = nodal + annual + semiannual;
// nodal:     -4.4 * sin(2π/18.61 * (year - 2015.6)) + 0.45
// annual:    +1.7 * sin(2π * (year + 0.27))
// semiannual: +0.9 * sin(4π * (year + 0.12))
```

### Extremum Refinement

1. **Coarse search:** 15-minute samples over 24 hours (96 samples)
2. **Peak detection:** Find local maxima/minima
3. **Ternary search:** Refine to 30-second accuracy

## Astronomical Calculations

### Moon Phase (Meeus Algorithm)

- **Input:** Julian Date
- **Output:** Phase 0-1 (0=new, 0.5=full)
- **Accuracy:** ~1 minute for full/new moon times

### Sunrise/Sunset (NOAA Algorithm)

- **Location:** Margate (51.3813°N, 1.3862°E)
- **Zenith:** 90.833° (accounts for refraction)
- **Output:** Local time (with BST adjustment)

## Display Driver (SSD1680)

### Tri-color Handling

The SSD1680 is a BWR (black/white/red) display. To suppress red:

```c
// Write inverted data to BLACK RAM (0x24)
u8x8_cad_SendCmd(u8x8, 0x24);
u8x8_cad_SendData(u8x8, 8, ~tile_data);

// Write all zeros to COLOR RAM (0x26) at SAME offset
u8x8_cad_SendCmd(u8x8, 0x26);
u8x8_cad_SendData(u8x8, 8, zeros);
```

**Key insight:** Color RAM must be written with the same tile offset as black RAM, otherwise ghosting/red tint appears.

### Refresh Modes

Three refresh modes are available:

| Mode | Command | Time | Flashes | Use Case |
|------|---------|------|---------|----------|
| Full (OTP) | 0xF7 | ~3s | Multiple | Best contrast, clears ghosting |
| Fast Full | 0xC7 | ~1s | Single | Regular updates, slight ghosting over time |
| Partial | 0x0F | ~0.3s | None | Small changes only, requires base image |

**Full Refresh (Original/Default)**
- Uses OTP (One-Time Programmable) waveform stored in display
- Multiple black-white-black flashes
- Best image quality and contrast
- Clears any accumulated ghosting
- Use: First boot, clearing ghosting (e.g., nightly)

**Fast Full Refresh (Recommended for regular updates)**
- Uses custom LUT (Look-Up Table) from Waveshare EPD_2IN9_V2_Init_Fast()
- Single flash (black → white → final image)
- Good contrast, ~3x faster than full refresh
- Slight ghosting accumulation over many updates
- Use: Regular 15-minute updates

**Partial Refresh**
- Uses custom partial LUT
- No flash, only changed pixels update
- Fastest but lowest contrast
- Requires base image in both RAM banks
- Use: Not recommended for this display (poor results)

### Refresh Mode API

```c
// Set refresh mode before calling display_update()
display_set_full_mode();       // OTP waveform, multiple flashes (~3s)
display_set_fast_full_mode();  // Custom LUT, single flash (~1s)
display_set_partial_mode();    // Partial update (~0.3s)

// Example usage
display_set_fast_full_mode();
display_clear();
render_tide_display(current_time, message);
display_update(NULL);
```

### BUSY Pin Handling

The BUSY pin (PA9) indicates display refresh status:
- **HIGH** = Display is busy (refreshing)
- **LOW** = Display is ready

Wait sequence after sending refresh command:
```c
// Wait for BUSY to go HIGH (refresh started)
while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_RESET) {
    // timeout after 500ms
}

// Wait for BUSY to go LOW (refresh complete)
while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) {
    __WFI();  // Light sleep, wake on SysTick (1ms)
}
```

### Production Refresh Strategy

For battery-powered operation with ~15 min update intervals:
- Use `display_set_fast_full_mode()` for all regular updates
- Do one `display_set_full_mode()` update per night (e.g., 3am) to clear ghosting

```c
// In main update loop
if (local->tm_hour == 3 && local->tm_min < 15) {
    display_set_full_mode();  // Nightly full refresh
} else {
    display_set_fast_full_mode();  // Fast refresh
}
```

## Power Optimization

### During Display Refresh

```c
// Use __WFI() for light sleep while polling BUSY
while (HAL_GPIO_ReadPin(EPD_PORT, EPD_BUSY_PIN) == GPIO_PIN_SET) {
    __WFI();  // Wake every 1ms on SysTick
}
```

### Between Updates

- **STOP mode:** ~uA current
- **Wake source:** EXTI0 (DS3231 alarm via P-FET)
- **Cycle time:** 15 minutes

## Memory Usage

```
RAM:    5,776 bytes (4.4% of 128KB)
Flash: 75,780 bytes (14.5% of 512KB)
```

Breakdown:
- u8g2 framebuffer: ~5KB (128x296/8 = 4,736 bytes)
- Tide constituents: ~400 bytes (31 × 12 bytes)
- Stack/heap: ~600 bytes

---

# Hardware Test Documentation

PlatformIO project using STM32Cube HAL framework.

## Completed Tests (HAL)

### E-Paper Display (SSD1680 296x128 BWR)
- Full refresh working with WeAct 2.9" display
- X RAM address offset: 1-16 (WeAct specific)
- BWR displays don't support fast partial refresh

### DS3231 RTC
- I2C communication working (address 0x68)
- Time set/get working
- Alarm1 working (hour:min:sec match)

### Sleep/Wakeup Cycle
- STOP mode working (deep sleep, LEDs off)
- Wake via PA0 (DS3231 INT through P-FET -> rising edge)
- PA0 state detection on boot to distinguish fresh boot vs alarm wakeup
- Alarm flags cleared after wakeup

### Button Time Setting
- Button on PA2 for user input
- Hold on boot to enter time-setting mode
- Short presses cycle through hours/minutes

---

## Pin Configuration

| Pin  | Function                          |
|------|-----------------------------------|
| PA0  | Wakeup (DS3231 INT via MOSFET)    |
| PA1  | Gate: DS3231 VCC P-FET (LOW=ON)   |
| PA2  | Button input (active LOW)         |
| PA3  | Gate: Display VCC P-FET (LOW=ON)  |
| PA4  | Display CS                        |
| PA5  | Display CLK                       |
| PA6  | Display DC                        |
| PA7  | Display DIN                       |
| PA8  | Display RST                       |
| PA9  | Display BUSY                      |
| PA10 | DS3231 RST (power status input)   |
| PB6  | I2C1 SCL (DS3231)                 |
| PB7  | I2C1 SDA                          |
| PC11 | LED2 (active HIGH)                |
| PC14 | DS3231 32kHz out (LSE input)      |
| PD2  | LED1 (active HIGH)                |

## ST-Link Connection

```
ST-Link    STM32F411
-------    ---------
SWDIO  --> PA13
SWCLK  --> PA14
GND    --> GND
3.3V   --> 3V3 (optional)
NRST   --> NRST (optional, helps recovery)
```

## Troubleshooting

**Can't upload:**
- Check ST-Link connections (SWDIO, SWCLK, GND)
- Try holding RESET while starting upload
- Check if BOOT0 is LOW (should be for normal boot)

**Display shows red tint:**
- Ensure color RAM (0x26) is written with zeros at same offset as black RAM
- Check tile-by-tile writing includes color RAM

**Device not waking from sleep:**
- Check DS3231 alarm flags are cleared before sleep
- Verify EXTI is configured for rising edge (P-FET inverts INT signal)
- Ensure PA0 has pull-down resistor

# STM32 Port Instructions for Tide Display

## Target Hardware
- **MCU**: STM32F411 (512KB flash, 128KB RAM, FPU)
- **Display**: WeAct 2.9" e-paper (296x128, SSD1680 controller)
- **RTC**: DS3231 module (optional but recommended for accuracy)

## Hardware Connections

| WeAct E-Paper | STM32F411 |
|---------------|-----------|
| VCC           | 3.3V      |
| GND           | GND       |
| DIN (MOSI)    | PA7 (SPI1)|
| CLK (SCK)     | PA5 (SPI1)|
| CS            | PA4       |
| DC            | PA3       |
| RST           | PA2       |
| BUSY          | PA1       |

## Project Setup

Create a PlatformIO project with:

```ini
; platformio.ini
[env:blackpill_f411ce]
platform = ststm32
board = blackpill_f411ce
framework = arduino
lib_deps =
    olikraus/U8g2@^2.35.9
    adafruit/RTClib@^2.1.1
build_flags =
    -D U8X8_WITH_USER_PTR
```

## Files to Create

### 1. `src/tide_calculator.h` and `src/tide_calculator.cpp`
Port the `TideCalculator` class from `epaper_sim.py`:
- Constructor sets up harmonic constituents (M2, S2, N2, K1)
- `calculate_tide_height(time_t dt)` returns float height in meters
- `find_next_high_low(time_t dt)` returns structs with time and height
- Use `sinf()` and `cosf()` - the FPU handles these efficiently, no lookup tables needed

Key constants:
```cpp
const float MEAN_SEA_LEVEL = 2.1f;  // meters
struct Constituent {
    float amplitude;
    float period;  // hours
};
// M2: {1.7, 12.42}, S2: {0.5, 12.0}, N2: {0.4, 12.66}, K1: {0.3, 23.93}
```

### 2. `src/moon_calculator.h` and `src/moon_calculator.cpp`
Port the `MoonCalculator` class:
- `calculate_moon_phase(time_t dt)` returns float 0-1 (0=new, 0.5=full)
- `find_next_full_new_moon(time_t dt)` returns two time_t values
- Reference: known new moon Jan 11, 2024 11:57 UTC
- Lunar cycle: 29.53059 days

### 3. `src/sun_calculator.h` and `src/sun_calculator.cpp`
Port the `SunCalculator` class:
- `calculate_sunrise_sunset(time_t dt, int* sunrise_mins, int* sunset_mins)`
- Returns minutes since midnight for sunrise/sunset
- Margate coordinates: lat 51.3813, lon 1.3862

### 4. `src/messages.h`
Replace CSV with a struct array:
```cpp
struct SpecialMessage {
    uint8_t day;
    uint8_t month;
    const char* message;
};

const SpecialMessage MESSAGES[] = {
    {1, 1, "Happy New Year!"},
    {14, 2, "Happy Valentine's Day!"},
    {17, 3, "Happy St Patrick's Day!"},
    {1, 4, "April Fools!"},
    {21, 6, "Happy Summer Solstice!"},
    {31, 10, "Happy Halloween!"},
    {25, 12, "Merry Christmas!"},
    {31, 12, "Happy New Year's Eve!"},
    // Add personal dates here
};

const char* get_message(uint8_t day, uint8_t month);
```

### 5. `src/display.h` and `src/display.cpp`
Display rendering using U8g2:
- Initialize U8g2 for SSD1680: `U8G2_SSD1680_296X128_F_4W_HW_SPI u8g2(U8G2_R1, CS, DC, RST);`
- Port `draw_moon_phase()` - use `u8g2.drawCircle()`, `u8g2.drawDisc()`, and pixel-by-pixel for terminator
- Port daylight bar - `u8g2.drawBox()` for filled rectangles
- Port tide bar and sine wave
- Fonts to use: `u8g2_font_helvB24_tr` (large), `u8g2_font_helvR14_tr` (medium), `u8g2_font_helvR10_tr` (small)

### 6. `src/main.cpp`
Main loop with sleep:

```cpp
#include <Arduino.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include "tide_calculator.h"
#include "moon_calculator.h"
#include "sun_calculator.h"
#include "display.h"
#include "messages.h"

// Initialize peripherals
// Read RTC time
// Calculate all values
// Render display
// Set next RTC alarm (e.g., 1 hour later or next tide event)
// Enter standby mode: HAL_PWR_EnterSTANDBYMode();
```

## Low Power Implementation

For deep sleep with RTC wakeup:

```cpp
#include <STM32LowPower.h>

void setup() {
    LowPower.begin();
    LowPower.enableWakeupFrom(&RTC, alarmCallback);
}

void loop() {
    // ... update display ...

    // Set alarm for next wakeup (e.g., 1 hour)
    RTC.setAlarm1(nextWakeTime, DS3231_A1_Hour);

    // Enter deep sleep (~3-5 µA)
    LowPower.deepSleep();
}
```

Add to platformio.ini:
```ini
lib_deps =
    ...
    stm32duino/STM32duino Low Power@^1.2.4
```

## Porting Notes

### Python to C++ Syntax Changes
- `datetime` → `time_t` or `DateTime` from RTClib
- `timedelta(hours=x)` → just integer arithmetic (seconds or minutes)
- `math.sin(x)` → `sinf(x)`
- `math.pi` → `M_PI` or `3.14159265f`
- List comprehensions → for loops
- `self.` → `this->` or just member variables

### Display Coordinate System
- Same as Python: 128x296 pixels, origin top-left
- U8g2 handles rotation with `U8G2_R1` (90 degrees)

### Testing Strategy
1. First test calculators on desktop (compile with g++)
2. Then test display rendering with U8g2 (without sleep)
3. Finally add sleep/wake cycle

## Reference: Python Source Files
- Calculator logic: `epaper_sim.py` lines 13-164 (TideCalculator, MoonCalculator, SunCalculator)
- Display rendering: `epaper_sim.py` lines 403-775 (TideDisplay.render_tide_info)
- Messages: `messages.csv`

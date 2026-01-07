# SSD1680 Tri-Color (BWR) Driver

u8g2 driver for 2.9" 296x128 tri-color e-paper displays (black/white/red).

## Tested Hardware

- WeAct 2.9" tri-color e-paper display

## Usage

```c
#include "u8g2.h"

// Declare the setup function
void u8g2_Setup_ssd1680_296x128_bwr_f(u8g2_t *u8g2, const u8g2_cb_t *rotation,
                                       u8x8_msg_cb byte_cb, u8x8_msg_cb gpio_and_delay_cb);

// Initialize
u8g2_t u8g2;
u8g2_Setup_ssd1680_296x128_bwr_f(&u8g2, U8G2_R0, u8x8_byte_stm32_hw_spi, u8x8_gpio_and_delay_stm32);
u8g2_InitDisplay(&u8g2);
u8g2_SetPowerSave(&u8g2, 0);
```

## Key Implementation Details

### Suppressing Red

BWR displays have two RAM areas:
- **Black RAM (0x24)**: Controls black pixels (0 = black, 1 = white)
- **Color RAM (0x26)**: Controls red pixels (1 = red)

To suppress red and get pure black/white:
1. Write inverted image data to Black RAM
2. Write all zeros to Color RAM **at the same offset** (per-tile)

### Refresh Mode

Uses 0xF7 update command for proper tri-color waveform. Refresh time is ~15 seconds.

### WeAct Display Offset

The WeAct display requires a +1 page offset on the Y axis.

## Notes

- This driver is archived for reference
- For monochrome displays, use the main `u8x8_d_ssd1680_296x128.c` driver

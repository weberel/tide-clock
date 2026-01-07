# Firmware

STM32 firmware for the tide clock. Two hardware revisions are supported.

## Building

```bash
# For Rev A hardware
cd revA
cd lib && git clone https://github.com/olikraus/u8g2.git && cd ..
pio run
pio run --target upload  # via ST-Link

# For Rev B hardware
cd revB
cd lib && git clone https://github.com/olikraus/u8g2.git && cd ..
pio run
pio run --target upload
```

## Configuration

Location settings are in `include/location_config.h`. Generate with:

```bash
cd scripts
python3 generate_location.py --noaa 9414290 --name "San Francisco" --tz US_PACIFIC
```

See the scripts README for more options.

## Hardware Differences

See [kicad/versions.md](../kicad/versions.md) for differences between Rev A and Rev B.

## Key Files

| File | Description |
|------|-------------|
| `src/main.c` | Entry point, power management |
| `src/tide.c` | Harmonic tide prediction |
| `src/astro.c` | Moon phase, sunrise/sunset |
| `src/timezone.c` | DST handling |
| `src/render.c` | Display layout |
| `include/location_config.h` | Location parameters (generated) |

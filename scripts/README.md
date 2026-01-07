# Scripts

Python scripts for generating location configuration.

## generate_location.py

Generates `location_config.h` for firmware and simulator.

### US Locations (using NOAA)

```bash
# Search for stations
python3 fetch_noaa.py --search "San Francisco"
python3 fetch_noaa.py --state CA

# Generate config
python3 generate_location.py --noaa 9414290 --name "San Francisco" --tz US_PACIFIC
```

### Other Locations (using fitted model)

```bash
python3 generate_location.py \
    --json harmonic_fitting/margate_nodal_fast.json \
    --name "Margate" \
    --lat 51.38 --lon 1.39 \
    --tz UK
```

## fetch_noaa.py

Fetches harmonic constituents from NOAA CO-OPS API.

```bash
python3 fetch_noaa.py 9414290              # Fetch by station ID
python3 fetch_noaa.py --search "Boston"    # Search by name
python3 fetch_noaa.py --state MA           # List state stations
python3 fetch_noaa.py 9414290 -o sf.json   # Save to file
```

## Supported Timezones

| Rule | Offset | Description |
|------|--------|-------------|
| `UTC` | +0 | No DST |
| `UK` | +0/+1 | GMT/BST |
| `EU_CENTRAL` | +1/+2 | CET/CEST |
| `EU_EASTERN` | +2/+3 | EET/EEST |
| `US_EASTERN` | -5/-4 | EST/EDT |
| `US_CENTRAL` | -6/-5 | CST/CDT |
| `US_MOUNTAIN` | -7/-6 | MST/MDT |
| `US_PACIFIC` | -8/-7 | PST/PDT |
| `US_ALASKA` | -9/-8 | AKST/AKDT |
| `US_HAWAII` | -10 | HST (no DST) |
| `AU_EASTERN` | +10/+11 | AEST/AEDT |
| `AU_WESTERN` | +8 | AWST (no DST) |
| `NZ` | +12/+13 | NZST/NZDT |

## harmonic_fitting/

Scripts for fitting harmonic constituents from tide data. See [harmonic_fitting/README.md](harmonic_fitting/README.md).

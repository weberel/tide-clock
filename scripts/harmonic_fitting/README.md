# Harmonic Fitting

Fits tidal harmonic constituents from observed tide data. Produces a JSON model file for use with `generate_location.py`.

## Data Format

Place your tide data in the `data/` folder. The scripts expect:

### Minute-by-minute data (for fitting)

CSV files with columns:
```
datetime,height_m
2019-01-01 00:00:00,2.45
2019-01-01 00:01:00,2.46
...
```

### High/low water data (for validation)

CSV file with columns:
```
datetime,height_m,type
2019-01-01 06:23:00,4.82,HW
2019-01-01 12:45:00,0.58,LW
...
```

## Scripts

### fit_nodal_fast.py

Fits 31 harmonic constituents with nodal corrections to minute data.

```bash
cd scripts/harmonic_fitting
python3 fit_nodal_fast.py
```

Output: `margate_nodal_fast.json`

### fit_time_correction.py

Fits empirical time correction (nodal cycle + seasonal) using HWLW data.

```bash
python3 fit_time_correction.py [--dataset pla_2019_2022|pla_2019_2026|pla_all]
```

Output: `margate_time_correction.json`

## Generating Location Config

After fitting, generate firmware config:

```bash
cd scripts
python3 generate_location.py \
    --json harmonic_fitting/margate_nodal_fast.json \
    --name "Your Location" \
    --lat 51.38 --lon 1.39 \
    --tz UK
```

## Included Data

`data/margate/` contains:
- Minute data 2019-2026 (Port of London Authority)
- HWLW predictions 2010-2040

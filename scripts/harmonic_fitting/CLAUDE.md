# Margate Tide Prediction - Harmonic Fitting

This directory contains code for predicting tide times at Margate, UK using harmonic analysis.

## Project Purpose

Build a tide prediction model for an e-paper display gift that shows upcoming high/low tide times at Margate.

## Data Sources

**Training Data:**
- `Margate-PLA-2025-converted.csv` - 1,411 tide observations from Port of London Authority (2025)
- `archive/3_Data/2_Margate.txt` - 34,098 historical observations (1967-1995)
  - Format: `DD/MM/YYYY HH:MM:SS, height(m ODN), HW=1/LW=0` (3 columns)

**Reference Data:**
- Sheerness 15-minute water level data from BODC (nearby station)
- `sheerness_fitted_constituents.py` - constituents fitted using utide library

## Final Model (13 Constituents)

Located in `margate_enhanced.py` as `BEST_CONSTITUENTS`:

```python
BEST_CONSTITUENTS = {
    'M2':   {'amplitude': 1.588, 'phase': 336.2, 'speed': 28.9841042},  # Principal lunar
    'S2':   {'amplitude': 0.498, 'phase': 30.0,  'speed': 30.0000000},  # Principal solar
    'N2':   {'amplitude': 0.314, 'phase': 263.5, 'speed': 28.4397295},  # Lunar elliptic
    'O1':   {'amplitude': 0.185, 'phase': 201.0, 'speed': 13.9430356},  # Lunar diurnal
    'K1':   {'amplitude': 0.167, 'phase': 358.2, 'speed': 15.0410686},  # Lunisolar diurnal
    'MU2':  {'amplitude': 0.103, 'phase': 30.5,  'speed': 27.9682084},  # Variational
    'NU2':  {'amplitude': 0.099, 'phase': 14.5,  'speed': 28.5125831},  # Evectional
    '2N2':  {'amplitude': 0.094, 'phase': 89.5,  'speed': 27.8953548},  # Lunar elliptic
    'L2':   {'amplitude': 0.069, 'phase': 266.0, 'speed': 29.5284789},  # Smaller lunar elliptic
    '2SM2': {'amplitude': 0.043, 'phase': 320.2, 'speed': 31.0158958},  # Shallow water
    'T2':   {'amplitude': 0.033, 'phase': 38.8,  'speed': 29.9589333},  # Solar elliptic
    'M6':   {'amplitude': 0.014, 'phase': 8.8,   'speed': 86.9523127},  # Sixth-diurnal
    '2MN6': {'amplitude': 0.002, 'phase': 288.5, 'speed': 86.4079380},  # Shallow water
}
```

**Key change from earlier models:** K2 constituent removed (amplitude=0) - it was causing sensitivity to the 18.6-year lunar nodal cycle.

## Model Performance

| Dataset | MAE | Bias | Within 15 min |
|---------|-----|------|---------------|
| 2025 PLA (1,411 obs) | 9.2 min | +0.5 min | 81.5% |
| Historical 1967-95 (34,098 obs) | 14.2 min | -3.9 min | 62.8% |

## Three Models Compared

Grid search was run with different weightings of 2025 vs historical data:

| Model | 2025 MAE | Hist MAE | Notes |
|-------|----------|----------|-------|
| Previous BEST | 10.14 min | 14.74 min | K2=0.020, bias -0.99 min |
| 60/40 weighted | 8.80 min | 15.09 min | Overfit to 2025 |
| **40/60 weighted** | **9.17 min** | **14.21 min** | **Current BEST** |

The 40/60 model (favoring historical data) was chosen for best long-term stability.

## Key Files

- `margate_enhanced.py` - Main model with `BEST_CONSTITUENTS` and `predict_tide()` function
- `compare_models.py` - Compares all 3 models, generates comparison plots
- `fine_grid_search.py` - Grid search optimization script
- `plot_2025_errors.py` - Generates 2025 error analysis plot
- `fit_sheerness_utide.py` - Fits constituents from Sheerness BODC data

## Validation Plots

- `model_comparison.png` - Error time series for all 3 models (1967-2025)
- `model_comparison_summary.png` - Bar chart comparing MAE across models
- `2025_errors.png` - 2025 errors in minutes and % of tidal cycle

## Prediction Algorithm

```python
height = MSL + Σ amplitude_i * cos(speed_i * hours_since_epoch - phase_i)
```

Where:
- `MSL` = 2.69m (Mean Sea Level at Margate)
- `speed` = angular velocity in degrees/hour (astronomically determined)
- `phase` = phase lag in degrees (location-specific)
- `amplitude` = tidal amplitude in meters (location-specific)
- `epoch` = 1900-01-01 00:00:00

## C Implementation

**Note:** The C code now uses a **31-constituent model** fitted via least-squares to PLA minute data (2019-2026), which supersedes the 13-constituent Python model described above.

Located in `../c/src/tide.c`:

```c
float calculate_tide_height(time_t dt);  // Returns height in meters
void find_next_high_low(time_t dt, TideEvent *next_high, TideEvent *next_low);
float get_time_correction(int year, int month, int day);  // Empirical nodal/seasonal correction
```

**C Model Details:**
- 31 harmonic constituents (M2, S2, N2, K2, K1, O1, P1, Q1, M4, MS4, MN4, 2N2, MU2, NU2, L2, T2, R2, M6, 2MS6, 2MN6, M8, S4, S6, SA, SSA, MSF, MF, MM, J1, OO1, 2Q1)
- Epoch: 2019-01-01 00:09:00 UTC
- MSL: 2.64m (Chart Datum)
- Angular speeds pre-computed in radians for STM32 FPU efficiency
- Empirical correction for 18.61-year nodal cycle + seasonal effects (reduces MAE by ~16%)

**Performance vs PLA:**
- Timing accuracy: MAE 4.2 min (2019-2026)
- Height accuracy: ±10cm typical, ±30cm max

## Lessons Learned

1. **K2 Removal**: The K2 constituent varies with the 18.6-year lunar nodal cycle - including it causes overfitting to specific years
2. **Historical Weighting**: Using 40% 2025 / 60% historical weighting produces the most stable long-term model
3. **Grid Search**: Iterative refinement with progressively finer grids (3 rounds) converges well
4. **Bias vs MAE**: Low MAE is more important than low bias for practical accuracy

---

# Tide Data Extraction - Continuation Instructions

## Current Data Extraction Status (as of Dec 1, 2025)

| Year | Status | File |
|------|--------|------|
| 2021 | COMPLETE | `margate_pla_2021_corrected.csv` (1405 entries, 0 incomplete days) |
| 2022 | IN PROGRESS | `margate_pla_2022_v3.csv` (1202 entries, needs corrections) |
| 2024 | COMPLETE | `margate_pla_2024_corrected.csv` (1415 entries, 0 incomplete days) |
| 2025 | NOT STARTED | Need to get PDF and process |

## How the Extraction Process Works

1. **PDF to Images**: PDFs are converted to images in `pla-{year}/` directories
2. **Cropping**: Images are cropped to extract Margate columns, saved in `pla-{year}-cropped/`
3. **OCR Extraction**: Script `extract_tide_data_v3.py` runs OCR to extract data to CSV
4. **Manual Correction**: Compare CSV against cropped images to find/fix OCR errors

## Key Extraction Files

- `extract_tide_data_v3.py` - Main OCR extraction script
- `manual_corrections_2021.csv` - 18 corrections for 2021
- `manual_corrections_2024.csv` - 151 corrections for 2024
- `margate_pla_2021_corrected.csv` - Final corrected 2021 data
- `margate_pla_2024_corrected.csv` - Final corrected 2024 data
- `margate_pla_2022_v3.csv` - Current 2022 data (needs corrections)

## Page Structure for 2021/2022 PDFs

Pages 40-87 contain tide data:
- **Page 40-43**: January (40: days 1-7, 41: days 8-15, 42: days 16-22, 43: days 23-31)
- **Page 44-47**: February
- **Page 48-51**: March
- ... and so on (4 pages per month)

Each page has:
- `page_XXX_margate.png` - Tide times and heights
- `page_XXX_dates.png` - Day numbers for verification

## What Needs to Be Done for 2022

1. **Identify incomplete days** in `margate_pla_2022_v3.csv`:
   ```python
   # Run this to find incomplete days
   import csv
   from collections import defaultdict
   days = defaultdict(list)
   with open('margate_pla_2022_v3.csv', 'r') as f:
       for row in csv.DictReader(f):
           days[row['date']].append(row)
   for date in sorted(days.keys()):
       if len(days[date]) < 3:
           print(f"{date}: {len(days[date])} entries")
   ```

2. **For each incomplete day**:
   - Calculate page number: `page = 40 + ((month-1) * 4) + page_in_month`
   - Where page_in_month: 0 for days 1-7, 1 for days 8-15, 2 for days 16-22, 3 for days 23-31
   - Read the cropped image using Claude's Read tool (images are in `pla-2022-cropped/`)
   - Compare with CSV data to find missing entries
   - Add corrections to `manual_corrections_2022.csv`

3. **Apply corrections** by merging with original CSV, removing duplicates, sorting

## Data Format

Each tide entry has:
- `date`: YYYY-MM-DD
- `time`: HH:MM (24-hour format)
- `height`: meters (e.g., 4.5 for high water, 0.8 for low water)
- `type`: HW (high water) or LW (low water)

## Important Notes

- Each day normally has 4 tides (~12h 25min apart)
- Some days have only 3 entries when the 4th tide crosses midnight to the next day - this is CORRECT
- HW heights typically range from 3.5-5.0m
- LW heights typically range from 0.3-1.9m
- Tides alternate HW/LW

## Example Correction Entry

```csv
date,time,height,type
2022-01-04,00:33,4.7,HW
2022-01-08,22:08,1.2,LW
```

## Command to Apply Corrections

```python
import csv
import pandas as pd

# Read original
df = pd.read_csv('margate_pla_2022_v3.csv')

# Read corrections
corrections = pd.read_csv('manual_corrections_2022.csv')

# Merge
df = pd.concat([df, corrections]).drop_duplicates(subset=['date', 'time']).sort_values(['date', 'time'])
df.to_csv('margate_pla_2022_corrected.csv', index=False)
```

## After 2022 is Complete

1. Get the 2025 PLA PDF (check PLA website or user may have it)
2. Convert to images, crop, extract using same process
3. Apply corrections if needed
4. Validate all 4 years are 100% complete

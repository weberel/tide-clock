#!/usr/bin/env python3
"""
Fit time correction parameters for tide predictions.

This script fits the empirical time correction that compensates for:
- 18.61-year lunar nodal cycle
- Annual seasonal variation
- Semi-annual seasonal variation

The correction is applied to predicted tide times to improve accuracy.

Usage:
    python fit_time_correction.py [--dataset DATASET]

Datasets:
    pla_2019_2022  - HWLW data 2019-2022 (default)
    pla_2019_2026  - HWLW data 2019-2026
    pla_all        - All HWLW data 2010-2040

Output:
    - margate_time_correction_fitted.json  - Fitted parameters
    - C code snippet for firmware
"""

import numpy as np
import csv
import json
import sys
from datetime import datetime, timedelta
from scipy.optimize import minimize
from pathlib import Path

# ============================================================================
# Configuration
# ============================================================================

NODAL_PERIOD = 18.61  # years (lunar nodal cycle)
EPOCH = datetime(2019, 1, 1, 0, 9, 0)  # Must match tide.c

# Firmware reference values (for comparison)
FIRMWARE_VALUES = {
    'NODAL_AMP': -4.922,
    'NODAL_PHASE': 2015.522,
    'NODAL_OFFSET': 0.424,
    'ANNUAL_AMP': 1.674,
    'ANNUAL_PHASE': -0.263,
    'SEMIANN_AMP': 0.699,
    'SEMIANN_PHASE': -0.129,
}

# Constituent data (from margate_nodal_fast.json)
CONSTITUENTS = None  # Loaded from JSON

# ============================================================================
# Load constituents
# ============================================================================

def load_constituents():
    """Load 31-constituent model from JSON."""
    global CONSTITUENTS
    with open('margate_nodal_fast.json', 'r') as f:
        CONSTITUENTS = json.load(f)
    print(f"Loaded {len(CONSTITUENTS)} constituents")

# ============================================================================
# Tide prediction (simplified, no nodal corrections - just for timing)
# ============================================================================

def predict_tide_height(dt):
    """Predict tide height at given datetime."""
    if CONSTITUENTS is None:
        load_constituents()

    hours = (dt - EPOCH).total_seconds() / 3600.0
    height = 2.64  # MSL

    for name, c in CONSTITUENTS.items():
        omega = c['speed_rad']  # radians per hour
        phase = np.radians(c['phase'])
        height += c['amplitude'] * np.cos(omega * hours - phase)

    return height

def find_extremum(dt, search_hours=7, is_high=True):
    """Find the next high or low tide after dt."""
    step = timedelta(minutes=1)
    best_t = dt
    best_h = predict_tide_height(dt)

    t = dt
    end = dt + timedelta(hours=search_hours)

    while t < end:
        h = predict_tide_height(t)
        if (is_high and h > best_h) or (not is_high and h < best_h):
            best_h = h
            best_t = t
        t += step

    return best_t, best_h

# ============================================================================
# Data loading
# ============================================================================

def load_pla_hwlw():
    """Load PLA high/low water predictions."""
    filepath = Path('data/margate/margate_hwlw_2010-2040.csv')
    data = []
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            dt = datetime.strptime(row['datetime'], '%Y-%m-%d %H:%M:%S')
            data.append({
                'datetime': dt,
                'height': float(row['height_m']),
                'type': row['type']
            })
    return data


def calculate_time_errors(observations, verbose=True):
    """Calculate time errors for each observation."""
    errors = []

    for i, obs in enumerate(observations):
        if verbose and i % 500 == 0:
            print(f"  Processing {i}/{len(observations)}...")

        is_high = obs['type'] == 'HW'

        # Search for predicted extremum near observed time
        search_start = obs['datetime'] - timedelta(hours=3)
        pred_t, pred_h = find_extremum(search_start, search_hours=6, is_high=is_high)

        # Time error in minutes (positive = prediction is late)
        error_min = (pred_t - obs['datetime']).total_seconds() / 60.0

        # Skip if error is too large (likely wrong extremum)
        if abs(error_min) > 60:
            continue

        errors.append({
            'datetime': obs['datetime'],
            'time_error': error_min,
            'type': obs['type']
        })

    return errors

def load_data(dataset='pla_2019_2022'):
    """Load and compute time errors for specified dataset."""
    print(f"Loading dataset: {dataset}")
    load_constituents()

    # Load raw data
    pla_data = load_pla_hwlw()
    print(f"  PLA data: {len(pla_data)} observations ({pla_data[0]['datetime'].year}-{pla_data[-1]['datetime'].year})")

    if dataset == 'pla_2019_2022':
        obs = [d for d in pla_data if 2019 <= d['datetime'].year <= 2022]
    elif dataset == 'pla_2019_2026':
        obs = [d for d in pla_data if 2019 <= d['datetime'].year <= 2026]
    elif dataset == 'pla_all':
        obs = pla_data
    else:
        raise ValueError(f"Unknown dataset: {dataset}")

    print(f"  Selected {len(obs)} observations")
    print("  Calculating time errors (this may take a few minutes)...")

    errors = calculate_time_errors(obs)
    print(f"  Computed {len(errors)} valid time errors")

    return errors

# ============================================================================
# Correction model
# ============================================================================

def make_correction_func(params):
    """Create a correction function from 7 parameters."""
    nodal_amp, nodal_phase, nodal_offset = params[0:3]
    annual_amp, annual_phase = params[3:5]
    semiann_amp, semiann_phase = params[5:7]

    def correction(decimal_year):
        nodal = nodal_amp * np.sin(2 * np.pi / NODAL_PERIOD * (decimal_year - nodal_phase)) + nodal_offset
        annual = annual_amp * np.sin(2 * np.pi * (decimal_year - annual_phase))
        semiann = semiann_amp * np.sin(4 * np.pi * (decimal_year - semiann_phase))
        return nodal + annual + semiann

    return correction

# ============================================================================
# Fitting
# ============================================================================

def fit_correction(obs, verbose=True):
    """Fit the 7-parameter correction model."""

    errors = np.array([o['time_error'] for o in obs])
    times = [o['datetime'] for o in obs]
    decimal_years = np.array([
        t.year + (t.month - 1) / 12.0 + (t.day - 1) / 365.25
        for t in times
    ])

    def objective(params):
        correction = make_correction_func(params)
        corrected = errors - correction(decimal_years)
        return np.mean(np.abs(corrected))

    # Multiple starting points to find global minimum
    starting_points = [
        # Close to firmware values
        [-4.9, 2015.5, 0.4, 1.7, -0.26, 0.7, -0.13],
        # Variations
        [-5.0, 2015.6, 0.5, 1.6, -0.25, 0.8, -0.12],
        [-4.8, 2015.4, 0.3, 1.8, -0.27, 0.6, -0.14],
        [-5.2, 2015.7, 0.6, 1.5, -0.24, 0.9, -0.11],
        [-4.6, 2015.3, 0.2, 1.9, -0.28, 0.5, -0.15],
    ]

    best_params = None
    best_mae = float('inf')

    if verbose:
        print("\nOptimizing from multiple starting points...")

    for i, x0 in enumerate(starting_points):
        result = minimize(objective, x0, method='Nelder-Mead',
                         options={'maxiter': 5000, 'xatol': 0.0001, 'fatol': 0.0001})

        if result.fun < best_mae:
            best_mae = result.fun
            best_params = result.x.copy()

        if verbose:
            print(f"  Start {i+1}: MAE={result.fun:.4f} min, NODAL_AMP={result.x[0]:.3f}")

    return best_params, best_mae

# ============================================================================
# Output
# ============================================================================

def print_results(params, mae, dataset):
    """Print fitted parameters and comparison to firmware."""

    names = ['NODAL_AMP', 'NODAL_PHASE', 'NODAL_OFFSET',
             'ANNUAL_AMP', 'ANNUAL_PHASE', 'SEMIANN_AMP', 'SEMIANN_PHASE']

    print("\n" + "=" * 70)
    print("FITTED PARAMETERS")
    print("=" * 70)
    print(f"Dataset: {dataset}")
    print(f"MAE: {mae:.4f} min")
    print()

    for name, value in zip(names, params):
        fw = FIRMWARE_VALUES[name]
        diff = value - fw
        match = "✓" if abs(diff) < 0.1 else ""
        print(f"  {name:<15} = {value:>8.3f}  (firmware: {fw:>8.3f}, diff: {diff:>+7.3f}) {match}")

def save_results(params, mae, dataset):
    """Save fitted parameters to JSON."""

    result = {
        "description": "Empirical time correction for Margate tide predictions",
        "units": "minutes (subtract from predicted time)",
        "fitted_on": datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        "dataset": dataset,
        "nodal": {
            "period_years": NODAL_PERIOD,
            "amplitude_min": float(params[0]),
            "phase_year": float(params[1]),
            "offset_min": float(params[2])
        },
        "annual": {
            "period_years": 1.0,
            "amplitude_min": float(params[3]),
            "phase_year": float(params[4])
        },
        "semiannual": {
            "period_years": 0.5,
            "amplitude_min": float(params[5]),
            "phase_year": float(params[6])
        },
        "performance": {
            "mae_min": float(mae)
        }
    }

    with open('margate_time_correction_fitted.json', 'w') as f:
        json.dump(result, f, indent=2)

    print(f"\nSaved: margate_time_correction_fitted.json")

def print_c_code(params):
    """Print C code snippet."""

    print("\n" + "=" * 70)
    print("C CODE FOR FIRMWARE")
    print("=" * 70)
    print(f"""
// Empirical Time Correction
// Fitted from PLA tide predictions

#define NODAL_AMP      {params[0]:.3f}f    // minutes
#define NODAL_PHASE    {params[1]:.3f}f    // year
#define NODAL_OFFSET   {params[2]:.3f}f    // minutes
#define NODAL_PERIOD   18.61f              // years

#define ANNUAL_AMP     {params[3]:.3f}f    // minutes
#define ANNUAL_PHASE   {params[4]:.3f}f    // year fraction

#define SEMIANN_AMP    {params[5]:.3f}f    // minutes
#define SEMIANN_PHASE  {params[6]:.3f}f    // year fraction

#define TWO_PI         6.28318530718f
#define NODAL_OMEGA    (TWO_PI / NODAL_PERIOD)

float get_time_correction(int year, int month, int day) {{
    float decimal_year = (float)year + (month - 1) / 12.0f + (day - 1) / 365.25f;

    float nodal = NODAL_AMP * sinf(NODAL_OMEGA * (decimal_year - NODAL_PHASE)) + NODAL_OFFSET;
    float annual = ANNUAL_AMP * sinf(TWO_PI * (decimal_year - ANNUAL_PHASE));
    float semiannual = SEMIANN_AMP * sinf(TWO_PI * 2.0f * (decimal_year - SEMIANN_PHASE));

    return nodal + annual + semiannual;
}}
""")

# ============================================================================
# Main
# ============================================================================

def main():
    print("=" * 70)
    print("MARGATE TIDE TIME CORRECTION FITTING")
    print("=" * 70)

    # Parse command line
    dataset = 'pla_2019_2022'  # Default: closest to firmware
    if len(sys.argv) > 1:
        if sys.argv[1] == '--dataset' and len(sys.argv) > 2:
            dataset = sys.argv[2]
        else:
            dataset = sys.argv[1]

    # Load data
    obs = load_data(dataset)

    # Fit
    params, mae = fit_correction(obs)

    # Calculate firmware MAE for comparison
    errors = np.array([o['time_error'] for o in obs])
    decimal_years = np.array([
        o['datetime'].year + (o['datetime'].month - 1) / 12.0 + (o['datetime'].day - 1) / 365.25
        for o in obs
    ])

    fw_params = [FIRMWARE_VALUES[n] for n in
                 ['NODAL_AMP', 'NODAL_PHASE', 'NODAL_OFFSET',
                  'ANNUAL_AMP', 'ANNUAL_PHASE', 'SEMIANN_AMP', 'SEMIANN_PHASE']]
    fw_correction = make_correction_func(fw_params)
    fw_corrected = errors - fw_correction(decimal_years)
    fw_mae = np.mean(np.abs(fw_corrected))

    # Print results
    print_results(params, mae, dataset)
    print(f"\n  Firmware MAE on this dataset: {fw_mae:.4f} min")
    print(f"  Fitted MAE:                   {mae:.4f} min")
    print(f"  Difference:                   {fw_mae - mae:.4f} min")

    # Save
    save_results(params, mae, dataset)

    # C code
    print_c_code(params)

    print("\n" + "=" * 70)
    print("COMPLETE")
    print("=" * 70)

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
Fast vectorized tidal fitting with nodal corrections.
Uses numpy vectorization instead of Python loops.
"""

import numpy as np
import csv
from datetime import datetime, timedelta
import json
from multiprocessing import Pool, cpu_count

# =============================================================================
# VECTORIZED NODAL CORRECTIONS
# =============================================================================

def get_N_vectorized(times_hours, epoch):
    """
    Calculate N (lunar ascending node longitude) for array of times.
    N decreases by ~19.328° per year.
    """
    ref_date = datetime(2000, 1, 1)
    epoch_years = (epoch - ref_date).total_seconds() / (365.25 * 24 * 3600)
    hours_per_year = 365.25 * 24

    # Convert hours since epoch to years since ref_date
    years = epoch_years + times_hours / hours_per_year

    N = (-19.3282 * years) % 360
    return np.radians(N)


def get_nodal_factors_vectorized(N_rad):
    """
    Vectorized nodal factor calculation.

    Args:
        N_rad: Array of N values in radians

    Returns:
        Dictionary of {'constituent': {'f': array, 'u': array}}
    """
    cos_N = np.cos(N_rad)
    sin_N = np.sin(N_rad)
    cos_2N = np.cos(2 * N_rad)
    sin_2N = np.sin(2 * N_rad)

    factors = {}

    # M2 group
    f_m2 = 1.0004 - 0.0373 * cos_N + 0.0002 * cos_2N
    u_m2 = -2.14 * sin_N  # degrees
    factors['M2'] = {'f': f_m2, 'u': np.radians(u_m2)}

    # S2 - no correction
    factors['S2'] = {'f': np.ones_like(N_rad), 'u': np.zeros_like(N_rad)}

    # N2, MU2, NU2, 2N2, L2, LAM2 - same as M2
    for name in ['N2', 'MU2', 'NU2', '2N2', 'L2', 'LAM2']:
        factors[name] = {'f': f_m2.copy(), 'u': np.radians(u_m2.copy())}

    # T2, 2SM2 - no correction
    for name in ['T2', '2SM2']:
        factors[name] = {'f': np.ones_like(N_rad), 'u': np.zeros_like(N_rad)}

    # K2
    f_k2 = 1.0241 + 0.2863 * cos_N + 0.0083 * cos_2N
    u_k2 = -17.74 * sin_N + 0.68 * sin_2N
    factors['K2'] = {'f': f_k2, 'u': np.radians(u_k2)}

    # K1
    f_k1 = 1.0060 + 0.1150 * cos_N - 0.0088 * cos_2N
    u_k1 = -8.86 * sin_N + 0.68 * sin_2N
    factors['K1'] = {'f': f_k1, 'u': np.radians(u_k1)}

    # O1
    f_o1 = 1.0089 + 0.1871 * cos_N - 0.0147 * cos_2N
    u_o1 = 10.80 * sin_N - 1.34 * sin_2N
    factors['O1'] = {'f': f_o1, 'u': np.radians(u_o1)}

    # P1 - no correction
    factors['P1'] = {'f': np.ones_like(N_rad), 'u': np.zeros_like(N_rad)}

    # Q1, 2Q1, RHO1 - same as O1
    for name in ['Q1', '2Q1', 'RHO1']:
        factors[name] = {'f': f_o1.copy(), 'u': np.radians(u_o1.copy())}

    # J1 - similar to K1
    factors['J1'] = {'f': 1.0060 + 0.1150 * cos_N, 'u': np.radians(-8.86 * sin_N)}

    # OO1
    factors['OO1'] = {'f': 1.0 + 0.436 * cos_N, 'u': np.radians(-18.6 * sin_N)}

    # Shallow water - compound from parents
    # M4 = M2^2
    factors['M4'] = {'f': f_m2 ** 2, 'u': 2 * np.radians(u_m2)}

    # MS4, MN4
    factors['MS4'] = {'f': f_m2, 'u': np.radians(u_m2)}
    factors['MN4'] = {'f': f_m2 ** 2, 'u': 2 * np.radians(u_m2)}

    # MK4
    factors['MK4'] = {'f': f_m2 * f_k2, 'u': np.radians(u_m2) + np.radians(u_k2)}

    # S4
    factors['S4'] = {'f': np.ones_like(N_rad), 'u': np.zeros_like(N_rad)}

    # M6, 2MN6, 2MS6
    factors['M6'] = {'f': f_m2 ** 3, 'u': 3 * np.radians(u_m2)}
    factors['2MN6'] = {'f': f_m2 ** 3, 'u': 3 * np.radians(u_m2)}
    factors['2MS6'] = {'f': f_m2 ** 2, 'u': 2 * np.radians(u_m2)}

    # MK3, 2MK3
    factors['MK3'] = {'f': f_m2 * f_k1, 'u': np.radians(u_m2) + np.radians(u_k1)}
    factors['2MK3'] = {'f': f_m2 ** 2 / f_k1, 'u': 2 * np.radians(u_m2) - np.radians(u_k1)}

    # Long period
    factors['Mf'] = {'f': 1.043 + 0.414 * cos_N, 'u': np.radians(-23.7 * sin_N)}
    factors['Mm'] = {'f': 1.0 - 0.130 * cos_N, 'u': np.zeros_like(N_rad)}

    return factors


# =============================================================================
# CONSTITUENT DATA
# =============================================================================

CONSTITUENT_SPEEDS = {
    'M2': 28.9841042, 'S2': 30.0000000, 'N2': 28.4397295, 'K2': 30.0821373,
    'MU2': 27.9682084, 'NU2': 28.5125831, '2N2': 27.8953548, 'L2': 29.5284789,
    'T2': 29.9589333, '2SM2': 31.0158958, 'LAM2': 29.4556253,
    'K1': 15.0410686, 'O1': 13.9430356, 'P1': 14.9589314, 'Q1': 13.3986609,
    'J1': 15.5854433, 'OO1': 16.1391017, '2Q1': 12.8542862, 'RHO1': 13.4715145,
    'M4': 57.9682084, 'MS4': 58.9841042, 'MN4': 57.4238337, 'MK4': 59.0662415, 'S4': 60.0000000,
    'M6': 86.9523127, '2MN6': 86.4079380, '2MS6': 87.9682084,
    'MK3': 44.0251729, '2MK3': 42.9271398,
    'Mf': 1.0980331, 'Mm': 0.5443747,
}


# =============================================================================
# DATA LOADING
# =============================================================================

def load_minute_data(filepath, start_year=None, end_year=None, subsample=1):
    """Load minute-by-minute tide data."""
    print(f"Loading data from {filepath}...")
    times, heights = [], []
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        count = 0
        for row in reader:
            count += 1
            if subsample > 1 and count % subsample != 0:
                continue
            dt = datetime.strptime(row['datetime'], '%Y-%m-%d %H:%M:%S')
            if start_year and dt.year < start_year:
                continue
            if end_year and dt.year > end_year:
                continue
            times.append(dt)
            heights.append(float(row['height_m']))
    print(f"  Loaded {len(times)} records from {times[0]} to {times[-1]}")
    return times, np.array(heights)


# =============================================================================
# FAST VECTORIZED FITTING
# =============================================================================

def fit_with_nodal_vectorized(times, heights, constituents, epoch=None):
    """
    Fast vectorized fit with nodal corrections.
    """
    if epoch is None:
        epoch = times[0]

    constituents = [c for c in constituents if c in CONSTITUENT_SPEEDS]

    # Convert times to hours since epoch
    t_hours = np.array([(t - epoch).total_seconds() / 3600.0 for t in times])
    n_obs = len(t_hours)
    n_const = len(constituents)

    print(f"Fitting {n_const} constituents to {n_obs} observations (vectorized)...")
    print(f"  Time span: {t_hours[-1]/24:.1f} days ({t_hours[-1]/24/365.25:.2f} years)")

    # Calculate nodal factors for all times at once
    print("  Computing nodal factors...")
    N_rad = get_N_vectorized(t_hours, epoch)
    nodal = get_nodal_factors_vectorized(N_rad)

    # Build design matrix (vectorized)
    print("  Building design matrix...")
    X = np.zeros((n_obs, 1 + 2 * n_const))
    X[:, 0] = 1  # MSL

    for i, name in enumerate(constituents):
        speed = CONSTITUENT_SPEEDS[name]
        omega = np.radians(speed)

        if name in nodal:
            f = nodal[name]['f']
            u = nodal[name]['u']
        else:
            f = np.ones(n_obs)
            u = np.zeros(n_obs)

        phase = omega * t_hours + u
        X[:, 1 + 2*i] = f * np.cos(phase)
        X[:, 2 + 2*i] = f * np.sin(phase)

    # Solve least squares
    print("  Solving least squares...")
    beta, residuals, rank, s = np.linalg.lstsq(X, heights, rcond=None)

    # Extract results
    msl = beta[0]
    results = {
        'msl': float(msl),
        'epoch': epoch.isoformat(),
        'nodal_corrected': True,
        'constituents': {}
    }

    print(f"\nMean sea level: {msl:.4f} m")
    print(f"\nEquilibrium Constituents:")
    print("-" * 70)

    amplitudes = []
    for i, name in enumerate(constituents):
        C = beta[1 + 2*i]
        S = beta[2 + 2*i]

        amplitude = np.sqrt(C**2 + S**2)
        phase = np.degrees(np.arctan2(S, C))
        if phase < 0:
            phase += 360

        amplitudes.append(amplitude)
        speed = CONSTITUENT_SPEEDS[name]

        results['constituents'][name] = {
            'amplitude': float(amplitude),
            'phase': float(phase),
            'speed': float(speed)
        }

    sorted_idx = np.argsort(amplitudes)[::-1]
    for i in sorted_idx[:20]:
        name = constituents[i]
        c = results['constituents'][name]
        print(f"{name:<8} {c['amplitude']:.4f} m   {c['phase']:.2f}°")

    # Fit statistics
    predictions = X @ beta
    residuals = heights - predictions
    rmse = np.sqrt(np.mean(residuals**2))
    r2 = 1 - np.var(residuals) / np.var(heights)

    print(f"\nFit statistics: RMSE {rmse*100:.2f} cm, R² {r2:.6f}")

    results['fit_stats'] = {'rmse_cm': float(rmse * 100), 'r2': float(r2)}

    return results


def predict_tide_nodal(t, model):
    """Predict tide height with nodal corrections."""
    epoch = datetime.fromisoformat(model['epoch'])
    hours = (t - epoch).total_seconds() / 3600
    height = model['msl']

    # Get nodal factors for this time
    N_rad = get_N_vectorized(np.array([hours]), epoch)
    nodal = get_nodal_factors_vectorized(N_rad)

    for name, params in model['constituents'].items():
        amp = params['amplitude']
        phase = params['phase']
        speed = params['speed']
        omega = np.radians(speed)

        if name in nodal:
            f = nodal[name]['f'][0]
            u = nodal[name]['u'][0]
        else:
            f, u = 1.0, 0.0

        height += f * amp * np.cos(omega * hours + u - np.radians(phase))

    return height


def find_extrema(start, end, model, step_min=10):
    """Find high and low tides."""
    extrema = []

    def pred(t):
        return predict_tide_nodal(t, model)

    t = start
    h_prev = pred(t)
    t += timedelta(minutes=step_min)
    h = pred(t)
    rising = h > h_prev

    while t <= end:
        h_prev_val, t_prev = h, t
        t += timedelta(minutes=step_min)
        if t > end:
            break
        h = pred(t)

        if rising and h < h_prev_val:
            best_t, best_h = t_prev, h_prev_val
            for dt in np.linspace(-step_min, step_min, 41):
                test_t = t_prev + timedelta(minutes=dt)
                test_h = pred(test_t)
                if test_h > best_h:
                    best_t, best_h = test_t, test_h
            extrema.append(('HW', best_t, best_h))
            rising = False

        elif not rising and h > h_prev_val:
            best_t, best_h = t_prev, h_prev_val
            for dt in np.linspace(-step_min, step_min, 41):
                test_t = t_prev + timedelta(minutes=dt)
                test_h = pred(test_t)
                if test_h < best_h:
                    best_t, best_h = test_t, test_h
            extrema.append(('LW', best_t, best_h))
            rising = True

    return extrema


# =============================================================================
# PARALLEL VALIDATION
# =============================================================================

def validate_single_obs(args):
    """Validate a single observation (for multiprocessing)."""
    obs, model = args
    extrema = find_extrema(obs['datetime'] - timedelta(hours=3),
                           obs['datetime'] + timedelta(hours=3), model)
    matches = [e for e in extrema if e[0] == obs['type']]
    if matches:
        _, pred_time, pred_height = min(matches, key=lambda x: abs((x[1] - obs['datetime']).total_seconds()))
        return (
            (pred_time - obs['datetime']).total_seconds() / 60,
            (pred_height - obs['height']) * 100
        )
    return None


def validate_hwlw_parallel(model, hwlw_file, start_year, end_year, max_events=10000, n_jobs=None):
    """Parallel validation against HWLW data."""
    print(f"\nValidating {start_year}-{end_year}...")

    observations = []
    with open(hwlw_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            dt = datetime.strptime(row['datetime'], '%Y-%m-%d %H:%M:%S')
            if start_year <= dt.year <= end_year:
                observations.append({
                    'datetime': dt,
                    'height': float(row['height_m']),
                    'type': row['type']
                })
                if len(observations) >= max_events:
                    break

    if n_jobs is None:
        n_jobs = cpu_count()

    # For small datasets, don't bother with multiprocessing
    if len(observations) < 100:
        results = [validate_single_obs((obs, model)) for obs in observations]
    else:
        # Use multiprocessing
        with Pool(n_jobs) as pool:
            results = pool.map(validate_single_obs, [(obs, model) for obs in observations])

    # Collect results
    time_errors = []
    height_errors = []
    for r in results:
        if r is not None:
            time_errors.append(r[0])
            height_errors.append(r[1])

    time_errors = np.array(time_errors)
    height_errors = np.array(height_errors)

    mae = np.mean(np.abs(time_errors))
    bias = np.mean(time_errors)
    within_15 = np.sum(np.abs(time_errors) <= 15) / len(time_errors) * 100
    height_rmse = np.sqrt(np.mean(height_errors**2))

    print(f"  {len(observations)} events: MAE {mae:.1f}min, bias {bias:+.1f}min, <15min {within_15:.1f}%, RMSE {height_rmse:.1f}cm")

    return mae, within_15, height_rmse


def validate_archive(model, archive_file, start_year, end_year):
    """Validate against historic archive data."""
    print(f"\nValidating historic {start_year}-{end_year}...")

    ODN_TO_CD = 2.69
    observations = []

    with open(archive_file, 'r') as f:
        next(f)
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(',')
            if len(parts) >= 3:
                try:
                    dt = datetime.strptime(parts[0].strip(), '%d/%m/%Y %H:%M:%S')
                except:
                    continue
                if start_year <= dt.year <= end_year:
                    observations.append({
                        'datetime': dt,
                        'height': float(parts[1].strip()) + ODN_TO_CD,
                        'type': 'HW' if int(parts[2].strip()) == 1 else 'LW'
                    })

    time_errors = []
    height_errors = []

    for obs in observations:
        extrema = find_extrema(obs['datetime'] - timedelta(hours=3),
                               obs['datetime'] + timedelta(hours=3), model)
        matches = [e for e in extrema if e[0] == obs['type']]
        if matches:
            _, pred_time, pred_height = min(matches, key=lambda x: abs((x[1] - obs['datetime']).total_seconds()))
            time_errors.append((pred_time - obs['datetime']).total_seconds() / 60)
            height_errors.append((pred_height - obs['height']) * 100)

    if not time_errors:
        print("  No valid comparisons")
        return None, None, None

    time_errors = np.array(time_errors)
    height_errors = np.array(height_errors)

    mae = np.mean(np.abs(time_errors))
    bias = np.mean(time_errors)
    within_15 = np.sum(np.abs(time_errors) <= 15) / len(time_errors) * 100
    height_rmse = np.sqrt(np.mean(height_errors**2))

    print(f"  {len(observations)} events: MAE {mae:.1f}min, bias {bias:+.1f}min, <15min {within_15:.1f}%, RMSE {height_rmse:.1f}cm")

    return mae, within_15, height_rmse


# =============================================================================
# MAIN
# =============================================================================

def main():
    print("=" * 70)
    print("FAST TIDE MODEL WITH NODAL CORRECTIONS")
    print(f"Using {cpu_count()} CPU cores")
    print("=" * 70)

    constituents = [
        'M2', 'S2', 'N2', 'K2', 'MU2', 'NU2', '2N2', 'L2', 'T2', '2SM2', 'LAM2',
        'K1', 'O1', 'P1', 'Q1', 'J1', 'OO1', '2Q1', 'RHO1',
        'M4', 'MS4', 'MN4', 'MK4', 'S4',
        'M6', '2MN6', '2MS6',
        'MK3', '2MK3',
        'Mf', 'Mm',
    ]

    # Load data
    times, heights = load_minute_data(
        'pla_minute/margate_minute_2019-2026.csv',
        start_year=2019, end_year=2026, subsample=10
    )

    # Fit with nodal corrections
    import time
    start = time.time()
    model = fit_with_nodal_vectorized(times, heights, constituents)
    print(f"\nFitting took {time.time() - start:.1f} seconds")

    # Save
    with open('margate_nodal_fast.json', 'w') as f:
        json.dump(model, f, indent=2)
    print("Saved: margate_nodal_fast.json")

    # Validate
    print("\n" + "=" * 70)
    print("VALIDATION")
    print("=" * 70)

    hwlw_file = 'pla_hwlw/margate_hwlw_2010-2040.csv'
    archive_file = 'archive/3_Data/2_Margate.txt'

    print("\n--- PLA Predictions ---")
    for start_y, end_y in [(2010, 2015), (2016, 2020), (2021, 2025), (2026, 2030), (2031, 2040)]:
        try:
            validate_hwlw_parallel(model, hwlw_file, start_y, end_y, max_events=5000)
        except Exception as e:
            print(f"  Error: {e}")

    print("\n--- Historic Archive ---")
    for start_y, end_y in [(1967, 1975), (1976, 1985), (1986, 1995)]:
        try:
            validate_archive(model, archive_file, start_y, end_y)
        except Exception as e:
            print(f"  Error: {e}")


if __name__ == '__main__':
    main()

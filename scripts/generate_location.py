#!/usr/bin/env python3
"""
Generate location_config.h for the tide clock.

Usage:
    # From fitted JSON (non-US locations):
    python generate_location.py --json harmonic_fitting/margate_nodal_fast.json \
        --name "Margate" --lat 51.3813 --lon 1.3862 --tz UK

    # From NOAA station (US locations):
    python generate_location.py --noaa 8518750 --name "New York" --tz US_EASTERN

    # List available timezone rules:
    python generate_location.py --list-tz
"""

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path

# Supported timezone rules
TZ_RULES = {
    'UTC':         {'winter': 0,  'summer': 0,  'desc': 'No DST'},
    'UK':          {'winter': 0,  'summer': 1,  'desc': 'GMT/BST - Last Sun Mar/Oct'},
    'EU_CENTRAL':  {'winter': 1,  'summer': 2,  'desc': 'CET/CEST - Last Sun Mar/Oct'},
    'EU_EASTERN':  {'winter': 2,  'summer': 3,  'desc': 'EET/EEST - Last Sun Mar/Oct'},
    'US_EASTERN':  {'winter': -5, 'summer': -4, 'desc': 'EST/EDT - 2nd Sun Mar / 1st Sun Nov'},
    'US_CENTRAL':  {'winter': -6, 'summer': -5, 'desc': 'CST/CDT - 2nd Sun Mar / 1st Sun Nov'},
    'US_MOUNTAIN': {'winter': -7, 'summer': -6, 'desc': 'MST/MDT - 2nd Sun Mar / 1st Sun Nov'},
    'US_PACIFIC':  {'winter': -8, 'summer': -7, 'desc': 'PST/PDT - 2nd Sun Mar / 1st Sun Nov'},
    'US_ALASKA':   {'winter': -9, 'summer': -8, 'desc': 'AKST/AKDT - 2nd Sun Mar / 1st Sun Nov'},
    'US_HAWAII':   {'winter': -10,'summer': -10,'desc': 'HST - No DST'},
    'AU_EASTERN':  {'winter': 10, 'summer': 11, 'desc': 'AEST/AEDT - 1st Sun Oct / 1st Sun Apr'},
    'AU_WESTERN':  {'winter': 8,  'summer': 8,  'desc': 'AWST - No DST'},
    'NZ':          {'winter': 12, 'summer': 13, 'desc': 'NZST/NZDT - Last Sun Sep / 1st Sun Apr'},
}

# Standard constituent names in order
CONSTITUENT_ORDER = [
    'M2', 'S2', 'N2', 'K2', 'K1', 'O1', 'P1', 'Q1',
    'MU2', 'NU2', '2N2', 'L2', 'T2', '2SM2', 'LAM2',
    'J1', 'OO1', '2Q1', 'RHO1',
    'M4', 'MS4', 'MN4', 'MK4', 'S4',
    'M6', '2MN6', '2MS6',
    'MK3', '2MK3',
    'Mf', 'Mm'
]

# Constituent descriptions
CONSTITUENT_DESC = {
    'M2': 'Principal lunar semidiurnal',
    'S2': 'Principal solar semidiurnal',
    'N2': 'Larger lunar elliptic',
    'K2': 'Lunisolar semidiurnal',
    'K1': 'Lunisolar diurnal',
    'O1': 'Principal lunar diurnal',
    'P1': 'Principal solar diurnal',
    'Q1': 'Larger lunar elliptic diurnal',
    'MU2': 'Variational',
    'NU2': 'Larger lunar evectional',
    '2N2': 'Lunar elliptic second order',
    'L2': 'Smaller lunar elliptic',
    'T2': 'Larger solar elliptic',
    '2SM2': '',
    'LAM2': 'Smaller lunar evectional',
    'J1': 'Smaller lunar elliptic diurnal',
    'OO1': 'Second order lunar diurnal',
    '2Q1': '',
    'RHO1': '',
    'M4': 'Shallow water overtide',
    'MS4': 'Shallow water quarter diurnal',
    'MN4': '',
    'MK4': '',
    'S4': '',
    'M6': 'Shallow water sixth diurnal',
    '2MN6': '',
    '2MS6': '',
    'MK3': 'Shallow water terdiurnal',
    '2MK3': '',
    'Mf': 'Lunar fortnightly',
    'Mm': 'Lunar monthly',
}


def load_json_constituents(json_path):
    """Load constituents from fitted JSON file."""
    with open(json_path) as f:
        data = json.load(f)

    constituents = []
    for name in CONSTITUENT_ORDER:
        if name in data['constituents']:
            c = data['constituents'][name]
            constituents.append({
                'name': name,
                'amplitude': c['amplitude'],
                'speed': c['speed'],
                'phase': c['phase'],
            })
        else:
            # Use zero amplitude for missing constituents
            constituents.append({
                'name': name,
                'amplitude': 0.0,
                'speed': 0.0,
                'phase': 0.0,
            })

    return {
        'constituents': constituents,
        'msl': data.get('msl', 0.0),
        'epoch': data.get('epoch', '2019-01-01T00:00:00'),
    }


def load_noaa_constituents(station_id):
    """Load constituents from NOAA CO-OPS API."""
    try:
        import urllib.request
        import ssl
    except ImportError:
        print("Error: urllib required for NOAA data fetch", file=sys.stderr)
        sys.exit(1)

    url = f"https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations/{station_id}/harcon.json"

    # Create SSL context that doesn't verify (for older Python)
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE

    try:
        with urllib.request.urlopen(url, context=ctx, timeout=30) as response:
            data = json.loads(response.read().decode())
    except Exception as e:
        print(f"Error fetching NOAA data: {e}", file=sys.stderr)
        sys.exit(1)

    if 'HarmonicConstituents' not in data:
        print(f"Error: No harmonic constituents found for station {station_id}", file=sys.stderr)
        sys.exit(1)

    # Build lookup by constituent name
    noaa_data = {}
    for c in data['HarmonicConstituents']:
        noaa_data[c['name']] = {
            'amplitude': float(c['amplitude']),
            'speed': float(c['speed']),
            'phase': float(c['phase_GMT']),
        }

    # Map to our standard order
    constituents = []
    for name in CONSTITUENT_ORDER:
        if name in noaa_data:
            c = noaa_data[name]
            constituents.append({
                'name': name,
                'amplitude': c['amplitude'],
                'speed': c['speed'],
                'phase': c['phase'],
            })
        else:
            constituents.append({
                'name': name,
                'amplitude': 0.0,
                'speed': 0.0,
                'phase': 0.0,
            })

    # Get station metadata for coordinates
    meta_url = f"https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations/{station_id}.json"
    try:
        with urllib.request.urlopen(meta_url, context=ctx, timeout=30) as response:
            meta = json.loads(response.read().decode())
        station = meta['stations'][0]
        lat = float(station['lat'])
        lon = float(station['lng'])
    except:
        lat = None
        lon = None

    return {
        'constituents': constituents,
        'msl': 0.0,  # NOAA uses different datum, would need conversion
        'epoch': '2000-01-01T00:00:00',  # NOAA uses J2000 epoch
        'lat': lat,
        'lon': lon,
    }


def epoch_to_unix(epoch_str):
    """Convert ISO epoch string to Unix timestamp."""
    dt = datetime.fromisoformat(epoch_str.replace('Z', '+00:00'))
    return int(dt.timestamp())


def generate_config(name, lat, lon, tz_rule, tide_data, corrections=None):
    """Generate location_config.h content."""

    tz = TZ_RULES[tz_rule]
    epoch_unix = epoch_to_unix(tide_data['epoch'])

    # Default corrections (zeros if not provided)
    if corrections is None:
        corrections = {
            'nodal_amp': 0.0,
            'nodal_phase': 2000.0,
            'nodal_offset': 0.0,
            'nodal_period': 18.61,
            'annual_amp': 0.0,
            'annual_phase': 0.0,
            'semiann_amp': 0.0,
            'semiann_phase': 0.0,
        }

    lines = []
    lines.append('/**')
    lines.append(' * ===========================================')
    lines.append(' * LOCATION CONFIGURATION')
    lines.append(' * ===========================================')
    lines.append(' *')
    lines.append(' * This file contains all location-specific parameters for the tide clock.')
    lines.append(' * To customize for a different location, regenerate this file using:')
    lines.append(' *')
    lines.append(' *   python scripts/generate_location.py --help')
    lines.append(' *')
    lines.append(f' * Location: {name}')
    lines.append(f' * Generated: {datetime.now().strftime("%Y-%m-%d")}')
    lines.append(' */')
    lines.append('')
    lines.append('#ifndef LOCATION_CONFIG_H')
    lines.append('#define LOCATION_CONFIG_H')
    lines.append('')
    lines.append('// ============================================================================')
    lines.append('// Location')
    lines.append('// ============================================================================')
    lines.append('')
    lines.append(f'#define LOCATION_NAME "{name}"')
    lines.append(f'#define LOCATION_LAT {lat}f')
    lines.append(f'#define LOCATION_LON {lon}f')
    lines.append('')
    lines.append('// ============================================================================')
    lines.append('// Timezone')
    lines.append('// ============================================================================')
    lines.append('// Supported rules: TZ_RULE_UTC, TZ_RULE_UK, TZ_RULE_EU_CENTRAL, TZ_RULE_EU_EASTERN,')
    lines.append('//                  TZ_RULE_US_EASTERN, TZ_RULE_US_CENTRAL, TZ_RULE_US_MOUNTAIN,')
    lines.append('//                  TZ_RULE_US_PACIFIC, TZ_RULE_US_ALASKA, TZ_RULE_US_HAWAII,')
    lines.append('//                  TZ_RULE_AU_EASTERN, TZ_RULE_AU_WESTERN, TZ_RULE_NZ')
    lines.append('')
    lines.append(f'#define TZ_RULE TZ_RULE_{tz_rule}')
    lines.append(f'#define TZ_OFFSET_WINTER {tz["winter"]}    // hours from UTC (standard time)')
    lines.append(f'#define TZ_OFFSET_SUMMER {tz["summer"]}    // hours from UTC (daylight saving time)')
    lines.append('')
    lines.append('// ============================================================================')
    lines.append('// Tidal Model')
    lines.append('// ============================================================================')
    lines.append('')
    lines.append(f'// Model epoch: {tide_data["epoch"]}')
    lines.append(f'#define TIDE_MODEL_EPOCH {epoch_unix}L')
    lines.append('')
    lines.append('// Mean sea level above chart datum (meters)')
    lines.append(f'#define TIDE_MEAN_SEA_LEVEL {tide_data["msl"]:.2f}f')
    lines.append('')
    lines.append('// Number of harmonic constituents')
    lines.append(f'#define TIDE_NUM_CONSTITUENTS {len(tide_data["constituents"])}')
    lines.append('')
    lines.append('// Harmonic constituents: {amplitude (m), speed (deg/hr), phase (deg)}')
    lines.append('// Constituents are stored in degrees and converted at init time')
    lines.append(f'static const float TIDE_CONSTITUENTS[{len(tide_data["constituents"])}][3] = {{')

    for c in tide_data['constituents']:
        desc = CONSTITUENT_DESC.get(c['name'], '')
        desc_str = f' - {desc}' if desc else ''
        lines.append(f'    {{{c["amplitude"]:.4f}f, {c["speed"]:.7f}f, {c["phase"]:7.3f}f}},  // {c["name"]}{desc_str}')

    lines.append('};')
    lines.append('')
    lines.append('// ============================================================================')
    lines.append('// Empirical Time Corrections')
    lines.append('// ============================================================================')
    lines.append('// These correct for the 18.61-year lunar nodal cycle plus seasonal effects.')
    lines.append('// Set to zero if not fitted for this location.')
    lines.append('')
    lines.append(f'#define NODAL_AMP      {corrections["nodal_amp"]:.3f}f    // minutes (amplitude of nodal correction)')
    lines.append(f'#define NODAL_PHASE    {corrections["nodal_phase"]:.3f}f  // year of zero crossing')
    lines.append(f'#define NODAL_OFFSET   {corrections["nodal_offset"]:.3f}f     // minutes (DC offset)')
    lines.append(f'#define NODAL_PERIOD   {corrections["nodal_period"]:.2f}f     // years (lunar nodal cycle)')
    lines.append(f'#define ANNUAL_AMP     {corrections["annual_amp"]:.3f}f     // minutes (annual seasonal amplitude)')
    lines.append(f'#define ANNUAL_PHASE  {corrections["annual_phase"]:.3f}f     // year fraction (annual phase)')
    lines.append(f'#define SEMIANN_AMP    {corrections["semiann_amp"]:.3f}f     // minutes (semi-annual amplitude)')
    lines.append(f'#define SEMIANN_PHASE {corrections["semiann_phase"]:.3f}f     // year fraction (semi-annual phase)')
    lines.append('')
    lines.append('#endif // LOCATION_CONFIG_H')
    lines.append('')

    return '\n'.join(lines)


def write_config(content, script_dir):
    """Write config to all three locations."""
    repo_root = script_dir.parent

    locations = [
        repo_root / 'firmware' / 'revA' / 'include' / 'location_config.h',
        repo_root / 'firmware' / 'revB' / 'include' / 'location_config.h',
        repo_root / 'simulator' / 'include' / 'location_config.h',
    ]

    for path in locations:
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)
        print(f"Written: {path}")


def main():
    parser = argparse.ArgumentParser(
        description='Generate location_config.h for the tide clock',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # From fitted JSON (UK/EU locations):
  python generate_location.py --json harmonic_fitting/margate_nodal_fast.json \\
      --name "Margate" --lat 51.3813 --lon 1.3862 --tz UK

  # From NOAA station (US locations):
  python generate_location.py --noaa 8518750 --name "New York" --tz US_EASTERN

  # List available timezones:
  python generate_location.py --list-tz
        """
    )

    parser.add_argument('--json', metavar='FILE', help='Load constituents from fitted JSON file')
    parser.add_argument('--noaa', metavar='STATION_ID', help='Load constituents from NOAA station')
    parser.add_argument('--name', required=False, help='Location name for display')
    parser.add_argument('--lat', type=float, help='Latitude (degrees, positive=North)')
    parser.add_argument('--lon', type=float, help='Longitude (degrees, positive=East)')
    parser.add_argument('--tz', choices=list(TZ_RULES.keys()), help='Timezone rule')
    parser.add_argument('--corrections', metavar='FILE', help='Load time corrections from JSON file')
    parser.add_argument('--list-tz', action='store_true', help='List available timezone rules')
    parser.add_argument('--dry-run', action='store_true', help='Print config without writing files')

    args = parser.parse_args()

    if args.list_tz:
        print("Available timezone rules:\n")
        for name, info in TZ_RULES.items():
            print(f"  {name:12}  UTC{info['winter']:+d}/{info['summer']:+d}  {info['desc']}")
        return

    if not args.json and not args.noaa:
        parser.error("Either --json or --noaa is required")

    if args.json and args.noaa:
        parser.error("Cannot use both --json and --noaa")

    # Load tide data
    if args.json:
        script_dir = Path(__file__).parent
        json_path = script_dir / args.json if not os.path.isabs(args.json) else Path(args.json)
        tide_data = load_json_constituents(json_path)

        if not args.lat or not args.lon:
            parser.error("--lat and --lon are required when using --json")
    else:
        tide_data = load_noaa_constituents(args.noaa)

        # Use NOAA coordinates if not specified
        if args.lat is None:
            args.lat = tide_data.get('lat')
        if args.lon is None:
            args.lon = tide_data.get('lon')

        if args.lat is None or args.lon is None:
            parser.error("Could not get coordinates from NOAA, please specify --lat and --lon")

    if not args.name:
        parser.error("--name is required")

    if not args.tz:
        parser.error("--tz is required")

    # Load corrections if provided
    corrections = None
    if args.corrections:
        script_dir = Path(__file__).parent
        corr_path = script_dir / args.corrections if not os.path.isabs(args.corrections) else Path(args.corrections)
        with open(corr_path) as f:
            corr_data = json.load(f)
        corrections = {
            'nodal_amp': corr_data.get('nodal', {}).get('amplitude_min', 0.0),
            'nodal_phase': corr_data.get('nodal', {}).get('phase_year', 2000.0),
            'nodal_offset': corr_data.get('nodal', {}).get('offset_min', 0.0),
            'nodal_period': corr_data.get('nodal', {}).get('period_years', 18.61),
            'annual_amp': corr_data.get('annual', {}).get('amplitude_min', 0.0),
            'annual_phase': corr_data.get('annual', {}).get('phase_year', 0.0),
            'semiann_amp': corr_data.get('semiannual', {}).get('amplitude_min', 0.0),
            'semiann_phase': corr_data.get('semiannual', {}).get('phase_year', 0.0),
        }

    # Generate config
    content = generate_config(args.name, args.lat, args.lon, args.tz, tide_data, corrections)

    if args.dry_run:
        print(content)
    else:
        script_dir = Path(__file__).parent
        write_config(content, script_dir)
        print(f"\nLocation '{args.name}' configured successfully!")
        print("Rebuild simulator/firmware to apply changes.")


if __name__ == '__main__':
    main()

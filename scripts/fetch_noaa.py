#!/usr/bin/env python3
"""
Fetch tidal harmonic constituents from NOAA CO-OPS API.

This script fetches harmonic constituents for any US tide station and outputs
them in JSON format compatible with generate_location.py.

Usage:
    # Fetch constituents for a station:
    python fetch_noaa.py 8518750

    # Search for stations by name:
    python fetch_noaa.py --search "New York"

    # List all stations in a state:
    python fetch_noaa.py --state NY

    # Output to JSON file:
    python fetch_noaa.py 8518750 -o new_york.json
"""

import argparse
import json
import sys
import urllib.request
import ssl
from datetime import datetime


def fetch_json(url):
    """Fetch JSON from URL."""
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE

    try:
        with urllib.request.urlopen(url, context=ctx, timeout=30) as response:
            return json.loads(response.read().decode())
    except urllib.error.HTTPError as e:
        print(f"HTTP Error {e.code}: {e.reason}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return None


def search_stations(query=None, state=None):
    """Search NOAA tide stations."""
    url = "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=harcon"

    data = fetch_json(url)
    if not data or 'stations' not in data:
        print("Error: Could not fetch station list", file=sys.stderr)
        return []

    stations = data['stations']

    # Filter by state
    if state:
        state = state.upper()
        stations = [s for s in stations if s.get('state', '').upper() == state]

    # Filter by name
    if query:
        query = query.lower()
        stations = [s for s in stations if query in s.get('name', '').lower()]

    return stations


def fetch_station_info(station_id):
    """Fetch station metadata."""
    url = f"https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations/{station_id}.json"
    data = fetch_json(url)

    if not data or 'stations' not in data or len(data['stations']) == 0:
        return None

    return data['stations'][0]


def fetch_harmonics(station_id):
    """Fetch harmonic constituents for a station."""
    url = f"https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations/{station_id}/harcon.json"
    data = fetch_json(url)

    if not data or 'HarmonicConstituents' not in data:
        return None

    return data['HarmonicConstituents']


def format_constituents(harmonics, station_info=None):
    """Format constituents as JSON compatible with generate_location.py."""
    constituents = {}
    for c in harmonics:
        constituents[c['name']] = {
            'amplitude': float(c['amplitude']),
            'speed': float(c['speed']),
            'phase': float(c['phase_GMT']),
        }

    result = {
        'source': 'NOAA CO-OPS',
        'fetched': datetime.now().isoformat(),
        'epoch': '2000-01-01T00:00:00',  # NOAA uses modified Julian date reference
        'msl': 0.0,  # Would need datum conversion
        'constituents': constituents,
    }

    if station_info:
        result['station'] = {
            'id': station_info.get('id'),
            'name': station_info.get('name'),
            'state': station_info.get('state'),
            'lat': float(station_info.get('lat', 0)),
            'lon': float(station_info.get('lng', 0)),
        }

    return result


def main():
    parser = argparse.ArgumentParser(
        description='Fetch tidal harmonic constituents from NOAA CO-OPS',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python fetch_noaa.py 8518750                    # Fetch The Battery, NY
  python fetch_noaa.py --search "San Francisco"  # Search for stations
  python fetch_noaa.py --state CA                # List California stations
  python fetch_noaa.py 9414290 -o sf.json        # Save to file

Popular US tide stations:
  8518750  The Battery, New York
  9414290  San Francisco, CA
  8443970  Boston, MA
  8658120  Wilmington, NC
  8723214  Virginia Key, FL
  9410660  Los Angeles, CA
  9447130  Seattle, WA
        """
    )

    parser.add_argument('station_id', nargs='?', help='NOAA station ID')
    parser.add_argument('--search', '-s', metavar='NAME', help='Search stations by name')
    parser.add_argument('--state', metavar='XX', help='List stations in state (2-letter code)')
    parser.add_argument('--output', '-o', metavar='FILE', help='Output JSON file')
    parser.add_argument('--pretty', '-p', action='store_true', help='Pretty-print JSON')

    args = parser.parse_args()

    # Search mode
    if args.search or args.state:
        stations = search_stations(query=args.search, state=args.state)

        if not stations:
            print("No stations found.", file=sys.stderr)
            sys.exit(1)

        print(f"Found {len(stations)} station(s):\n")
        for s in stations[:50]:  # Limit output
            print(f"  {s['id']:8}  {s['name']:<40}  {s.get('state', 'N/A'):>2}")

        if len(stations) > 50:
            print(f"\n  ... and {len(stations) - 50} more")

        return

    # Fetch mode
    if not args.station_id:
        parser.error("Station ID required (or use --search/--state)")

    print(f"Fetching data for station {args.station_id}...", file=sys.stderr)

    # Get station info
    station_info = fetch_station_info(args.station_id)
    if station_info:
        print(f"Station: {station_info.get('name', 'Unknown')}", file=sys.stderr)
        print(f"Location: {station_info.get('lat', '?')}°N, {station_info.get('lng', '?')}°W", file=sys.stderr)

    # Get harmonics
    harmonics = fetch_harmonics(args.station_id)
    if not harmonics:
        print(f"Error: Could not fetch harmonics for station {args.station_id}", file=sys.stderr)
        sys.exit(1)

    print(f"Found {len(harmonics)} harmonic constituents", file=sys.stderr)

    # Format output
    result = format_constituents(harmonics, station_info)

    indent = 2 if args.pretty else None
    json_output = json.dumps(result, indent=indent)

    if args.output:
        with open(args.output, 'w') as f:
            f.write(json_output)
        print(f"\nWritten to: {args.output}", file=sys.stderr)
    else:
        print(json_output)

    # Print usage hint
    if station_info:
        lat = station_info.get('lat', 0)
        lon = station_info.get('lng', 0)
        name = station_info.get('name', 'Unknown').split(',')[0]
        print(f"\nTo generate config, run:", file=sys.stderr)
        print(f"  python generate_location.py --noaa {args.station_id} --name \"{name}\" --tz US_EASTERN", file=sys.stderr)


if __name__ == '__main__':
    main()

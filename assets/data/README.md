# Bundled data files

## TLE snapshots (`*.tle`)
Offline fallbacks for the live CelesTrak feeds (`stations`, `gps-ops`,
`starlink-sample`). Used only when a live fetch fails and there's no fresh cache.

## City gazetteer (`cities15000.txt`) — required for location search

The F2 location picker searches a **GeoNames** city dump. This file is **not**
committed (it's a few MB); download it once:

1. Get `cities15000.zip` from GeoNames:
   https://download.geonames.org/export/dump/cities15000.zip
2. Unzip it and place `cities15000.txt` in this folder
   (`assets/data/cities15000.txt`).

`cities15000` = ~26k cities with population > 15,000. For more coverage you can
instead drop in `cities5000.txt` or `cities1000.txt` (same tab-separated format)
and rename it to `cities15000.txt`, or change the path in `src/main.cpp`.

If the file is missing the app still runs — the location picker just falls back
to manual `lat, lon` entry.

### Format
Tab-separated, 19 columns per the GeoNames `cities` schema. We read column 3
(asciiname), 5 (latitude), 6 (longitude), 9 (country code), 15 (population).

### Attribution (required)
GeoNames data is licensed **CC BY 4.0** — https://creativecommons.org/licenses/by/4.0/
Credit "GeoNames (geonames.org)" must appear in the app's About/credits and in
the release's THIRD-PARTY-LICENSES (see PLAN.md attribution backlog).

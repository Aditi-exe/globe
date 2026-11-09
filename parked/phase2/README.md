# Parked: Phase 2 (satellite data + propagation)

This is a **working prototype** of Phase 2, parked on 2026-11-08 so Phase 1
(Earth sphere + camera) could be finished first. It is **not compiled** by the
main build. Resurrect it when returning to Phase 2 instead of rebuilding.

## What's here
- `src/domain/CoordTransforms.{h,cpp}` — geodetic (lat/lon/alt) → unit-globe
  position; convention matched to the Earth texture UVs.
- `src/domain/SatelliteCatalog.{h,cpp}` — parse CelesTrak TLE files, propagate
  with SGP4 (libsgp4) to the current UTC time.
- `src/render/MarkerBatch.{h,cpp}` — draw satellites as GL_POINTS (amber dots).
- `assets/shaders/marker.{vert,frag}` — point-sprite marker shaders.
- `assets/data/*.tle` — bundled real TLE snapshots (stations / gps-ops /
  starlink-sample, 77 satellites) fetched from CelesTrak.

## How to bring it back (sketch)
1. Move these files back to their original locations under `src/` and `assets/`.
2. In `CMakeLists.txt`: re-add the libsgp4 FetchContent block (fetch + compile
   the `libsgp4/*.cpp` sources as a `sgp4` static lib; see git history / the
   plan), add the Marker/Coord/Catalog sources, and link `sgp4`.
3. In `src/main.cpp`: recreate the catalog, propagate each frame, upload marker
   positions, and draw them after the Earth.

## Verified before parking
- ISS (NORAD 25544) altitude came out 418.9 km — SGP4 + coordinate math correct.

## Still to do when resumed
- Live CelesTrak fetch (cpr) + 6h disk cache, bundled snapshot as offline fallback.
- Visually confirm continents/markers align (watch for east/west mirroring).
- Pin `libsgp4` and `stb` to specific commits for reproducibility.

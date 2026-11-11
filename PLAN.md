# Globe — Satellite Visualizer: Architecture & Plan

> A native (C++/OpenGL) real-time satellite visualizer. Fetches orbital data
> from a public source, propagates it locally, and renders satellites over a
> 3D Earth with info panels, ground tracks, and pass predictions based on the
> user's location. Distributed as a downloadable on GitHub, with a static
> download page later.

**Status:** Planning. Design is intentionally **open for review** — sections
marked 🔵 *OPEN FOR REVIEW* are deliberate decisions we expect to revisit.
Items marked ✅ were resolved in the 2026-10-27 review.

_Last updated: 2026-10-27_

---

## 1. Goals & non-goals

### Goals
- Native desktop app, **Windows-first**, code kept portable for later Mac/Linux.
- **Real-time** satellite positions computed **locally** (no mandatory server).
- 3D textured Earth with live satellite markers.
- Click a satellite → info panel (name, NORAD ID, country, type, live lat/lon/alt/velocity).
- Ground tracks / orbit paths for selected satellites.
- "Overhead now" + upcoming **pass predictions** from the user's location.
- A curated set of **major satellites rendered on the globe**; the full set
  browsable in a **searchable list/menu**.
- Ship as a **GitHub Release** download; add a **GitHub Pages** site later.
- **$0 recurring infrastructure** for v1 — no cloud database, no always-on server.

### Non-goals (v1)
- Web-first / browser version.
- User accounts, cloud sync, server-side history.
- Rendering every one of the ~10,000 active objects as 3D markers.
- A remote computation backend (kept *possible* via a seam, but not built).

---

## 2. Locked decisions

| Decision | Choice | Rationale |
|---|---|---|
| Platform (v1) | **Windows-first**, portable code | Matches dev machine; simplest to sign/distribute. |
| Computation | **Local** (SGP4 on client) | SGP4 is lightweight; avoids an always-on server + recurring cost. |
| Rendering API | **OpenGL 3.3 core** | Portable, well-documented, Mac-compatible ceiling (4.1). |
| Windowing/input | **GLFW** | Cross-platform, minimal, standard. |
| GL loader | **GLAD** | Standard, generated for 3.3 core. |
| Math | **GLM** | Matches GLSL conventions. |
| Orbit propagation | **libsgp4** (D. Warner) | Battle-tested SGP4/SDP4; confirm license on vendored build. |
| HTTP client | **cpr** (wraps libcurl) | Clean API; libcurl is the portable workhorse. |
| JSON parsing | **nlohmann/json** | Header-only, ergonomic. |
| Local storage | **SQLite** (embedded) + plain files | Zero-server structured local storage. |
| Data source (v1) | **CelesTrak** | Free, no API key, grouped TLE/JSON feeds. |
| UI | **Custom OpenGL UI** | User preference for a bespoke look. ⚠️ See §8 risk note. |
| Distribution | **GitHub Releases** + later **GitHub Pages** | Free binary + static hosting. |
| Database (cloud) | **None** | No stateful server features in scope. |

---

## 3. Data model & sources

### 3.1 What the data actually is
- **TLE (Two-Line Element set):** compact orbital elements per object, published
  by CelesTrak, refreshed ~daily. **Not a position** — you *propagate* it with
  SGP4 to get a position at any instant. "Real-time" = periodic TLE refresh +
  continuous local propagation.
- **SATCAT metadata:** name, NORAD catalog ID, country, launch date, object
  type, orbit class. Used for the info panel and the browsable list.

### 3.2 Sources (v1)
- **TLEs:** CelesTrak grouped feeds (e.g. `stations`, `starlink`, `gps-ops`,
  `active`) in JSON or 3LE format.
- **SATCAT:** CelesTrak SATCAT CSV/JSON.
- No API key required. **Respect CelesTrak fair-use / rate limits** — fetch on a
  throttled schedule and cache; never poll tightly.

### 3.3 Refresh & cache policy ✅ (window = 6h)
**Important:** the refresh window is **not** how fresh the positions are. A TLE
is an orbital *description*, not a position — SGP4 always propagates it to the
**current instant**, so a 6h-old (even days-old) TLE still yields an accurate
*right-now* position. The window only governs **how often we re-download** the
TLE file, as a politeness/bandwidth throttle against CelesTrak.

- TLEs re-downloaded when older than **6h**, or on explicit user refresh.
- On open: use cache immediately if present (positions propagated to `now`),
  fetch in the background if stale; fetch synchronously only if cache is empty.
- Cache to local disk so the app opens instantly and works briefly offline.
- SATCAT refreshed less often (weekly-ish) — it changes slowly.

---

## 4. High-level architecture

Layered, with a hard seam between *where data comes from* and *everything else*,
so local-vs-remote never leaks into the renderer.

```
                 ┌─────────────────────────────────────────────┐
                 │                  App / main loop             │
                 │  (window, timing, event dispatch, state)     │
                 └───────────────┬───────────────┬─────────────┘
                                 │               │
                 ┌───────────────▼────┐   ┌──────▼──────────────┐
                 │   Domain / Engine  │   │   Presentation      │
                 │                    │   │                     │
                 │  SatelliteCatalog  │   │  Renderer (OpenGL)  │
                 │  Propagator (SGP4) │   │   - Earth sphere    │
                 │  Observer/Location │   │   - Sat markers     │
                 │  PassPredictor     │   │   - Ground tracks   │
                 │  CoordTransforms   │   │  UI (custom OpenGL)  │
                 └─────────┬──────────┘   └─────────────────────┘
                           │
                 ┌─────────▼───────────────────────────────┐
                 │   Data access (the SEAM)                 │
                 │   ITleProvider  /  ISatcatProvider       │
                 │     ├── CelestrakProvider  (v1, HTTP)    │
                 │     └── RemoteProvider      (stub/later) │
                 │   Cache (disk) + Store (SQLite)          │
                 └──────────────────────────────────────────┘
```

### Data flow (one frame / tick)
1. On startup + every N hours: provider fetches TLEs (HTTP) → cache/store.
2. Each tick: `Propagator` runs SGP4 for tracked satellites at `now` → ECI.
3. `CoordTransforms` → ECEF → geodetic (lat/lon/alt) for the globe, and
   az/el relative to the `Observer` for overhead/pass logic.
4. `Renderer` draws Earth, the curated visible markers, and selected tracks.
5. `UI` draws panels (info, list/search, controls) and routes input.

---

## 5. Module breakdown

| Module | Responsibility | Key types |
|---|---|---|
| `platform/` | Window, GL context, input, timing | `Window`, `Clock`, `Input` |
| `data/` | The seam + fetching + caching | `ITleProvider`, `CelestrakProvider`, `RemoteProvider` (stub), `ISatcatProvider`, `Cache`, `Store` (SQLite) |
| `domain/` | Orbital model & math | `TleSet`, `Satellite`, `SatelliteCatalog`, `Propagator` (SGP4), `CoordTransforms`, `Observer`, `PassPredictor` |
| `render/` | OpenGL drawing | `Renderer`, `EarthSphere`, `MarkerBatch`, `TrackRenderer`, `Camera`, `Shader`, `Texture` |
| `ui/` | Custom OpenGL UI | `UiContext`, `TextRenderer`, `Panel`, `ListView`, `Button`, `SearchBox` |
| `app/` | Glue, state machine, config | `App`, `AppState`, `Config`, `Selection` |

### 5.1 The seam (why it matters)
```cpp
struct SatelliteState { /* id, eci pos/vel, timestamp */ };

class ITleProvider {
public:
    virtual ~ITleProvider() = default;
    virtual std::vector<TleSet> fetchGroup(std::string_view group) = 0;
    virtual bool isStale() const = 0;
};
```
- v1 ships `CelestrakProvider` (HTTP + SGP4 locally).
- `RemoteProvider` is a **stub** — if we ever want remote compute, we implement
  this one class and the renderer/UI never change.

---

## 6. Rendering plan (OpenGL 3.3 core)

- **Earth:** UV sphere mesh, textured with a public-domain **NASA Blue Marble**
  image (day map v1; optional night/clouds/specular later). Lat/lon grid optional.
- **Camera:** orbit/trackball camera (drag to rotate, scroll to zoom).
- **Satellite markers:** curated visible subset drawn as point sprites /
  instanced quads. Positions updated each tick from the propagator.
  ✅ **Visible subset (v1):** ISS/stations + GPS + a Starlink sample rendered on
  the globe; the full set lives in the searchable list (§5/§13). Marker style
  🔵 OPEN FOR REVIEW.
- **Ground tracks:** polyline of sub-satellite points (past + future arc) for
  the selected satellite; optional 3D orbit ellipse.
- **Selection:** ray-pick from cursor → nearest marker within a threshold.
- **Render order:** Earth → tracks → markers → UI (UI last, depth test off).

---

## 7. Orbital math pipeline

1. **Input:** TLE + timestamp.
2. **SGP4 (libsgp4):** → position/velocity in **ECI** (TEME frame).
3. **ECI → ECEF:** rotate by GMST (sidereal time) for the timestamp.
4. **ECEF → geodetic:** lat/lon/alt for placing markers on the globe.
5. **Observer look angles:** ECEF sat + ECEF observer → **azimuth/elevation**
   for "overhead now" and pass predictions. ✅ Observer location comes from
   **both** manual lat/lon entry **and** IP-based auto-detection (auto on first
   run, manually overridable). IP geolocation needs a keyless/low-friction
   lookup service — 🔵 pick the specific endpoint when we build §Phase 5.
6. **Pass prediction:** step time forward, find elevation rising above a
   threshold (e.g. 10°) → AOS/LOS/max-elevation windows.

> libsgp4 provides SGP4 + several of these transforms; we wrap them so the rest
> of the code is frame-agnostic.

---

## 8. Custom UI — plan & risk ⚠️

**User chose a custom OpenGL UI for a bespoke look. This is the biggest
complexity/schedule risk in the project** and a very common place to get stuck,
because a custom UI means we own:
- **Text rendering** (glyph atlas via **stb_truetype** or **FreeType**),
- **Input/event handling** (hover, click, focus, scroll, text entry),
- **Layout** (panels, lists, scrolling, z-order),
- **Theming** (the actual "fancy" styling).

### Plan
- Build a **small immediate-mode-style toolkit** scoped to exactly what we need:
  one info panel, a scrollable searchable list, a few buttons/toggles, a time
  control. No general-purpose UI framework ambitions.
- Start with **stb_truetype** for a single-font glyph atlas; upgrade to FreeType
  only if we need crisp multi-size/hinted text.

### ✅ RESOLVED (Phase 3, 2026-11-10) — staying custom
Fallback was **Dear ImGui** for panels if the custom UI ate the schedule. At the
Phase 3 checkpoint the hard part (stb_truetype text rendering + a minimal
immediate-mode overlay) came together cleanly, so **we're keeping the custom
toolkit** and building the list/search/time-control the same way. ImGui stays in
the back pocket only if a specific later widget proves disproportionately costly;
the 3D globe was always custom regardless.

---

## 9. Proposed project structure

```
globe/
├── CMakeLists.txt             # FetchContent: glfw/glm/glad (deps at build time)
├── PLAN.md                    # this file
├── README.md
├── LICENSE                    # e.g. MIT (TBD §12)
├── assets/
│   ├── textures/earth_day.jpg # NASA Blue Marble (public domain)
│   ├── shaders/*.vert|*.frag
│   └── fonts/*.ttf
├── src/
│   ├── main.cpp
│   ├── platform/              # Window, Input, Clock
│   ├── data/                  # ITleProvider, CelestrakProvider, Cache, Store
│   ├── domain/                # Propagator, CoordTransforms, Observer, PassPredictor
│   ├── render/                # Renderer, EarthSphere, MarkerBatch, TrackRenderer, Camera
│   ├── ui/                    # custom UI toolkit
│   └── app/                   # App, Config, AppState
├── third_party/               # libsgp4 (vendored) if not via package mgr
├── tests/                     # unit tests (propagation correctness, transforms)
└── .github/workflows/         # CI (build + release) later
```

---

## 10. Build & dependencies

- **CMake** (CLion-native) + **FetchContent** — deps are fetched & built at
  configure time; **no system install needed** (nothing added outside the build
  dir). ✅ *Changed from the original vcpkg plan:* vcpkg wasn't installed and we
  preferred not to add a system-wide tool. vcpkg remains an option later if we
  want prebuilt binary packages. Requires network on first configure + Python 3
  (GLAD generates its loader).
- Phase 0 deps (in `CMakeLists.txt`): `glfw` 3.4, `glm` 1.0.1, `glad` 2.0.6
  (GL 3.3 core). Later phases add: `cpr` (→ libcurl), `nlohmann-json`,
  `sqlite3`, `stb` (truetype/image); and `freetype` if the custom UI needs it.
- `libsgp4`: vendored in `third_party/` or via FetchContent — **confirm the
  license permits redistribution** before shipping.
- MSVC runtime set to dynamic across all targets (CMAKE_MSVC_RUNTIME_LIBRARY) to
  avoid LNK4098. Bundle required runtime DLLs with the Windows build.
- CMake 4.x note: `CMAKE_POLICY_VERSION_MINIMUM=3.5` is set so older-minimum
  deps (glfw/glm/glad) still configure.

---

## 11. Local storage

- **No cloud database.** All persistence is on the user's machine:
  - **TLE/SATCAT cache** — files on disk, timestamped for staleness.
  - **User settings + location + favorites** — **SQLite** file (or JSON if we
    keep it trivial).
- SQLite is embedded in the binary — no server, no setup, no cost.

---

## 12. Assets & licensing 🔵 OPEN FOR REVIEW

- **Earth imagery:** NASA Visible Earth / Blue Marble (public domain). Verify
  each specific image's terms before bundling.
- **Fonts:** ship only a redistributable font (e.g. an SIL OFL face).
- **libsgp4:** confirm its license allows redistribution in a binary.
- **Our code license:** default **MIT** (decide before first public release).
- **CelesTrak data:** respect fair-use; add attribution in About/README.

---

## 13. Milestones (phased)

**Phase 0 — Scaffolding** ✅ **DONE (2026-10-28)**
CMake + FetchContent (glfw/glm/glad), `Window` class with an OpenGL 3.3 core
context, clear-color frame loop (Esc to quit), module directory structure, CI
build placeholder, README, .gitignore. Verified: configures clean, builds with
no warnings on MSVC, and the executable launches and runs.

**Phase 1 — Earth & camera** ✅ **DONE (2026-11-08)**
Textured UV-sphere Earth, orbit/trackball camera (left-drag rotate, scroll zoom,
Esc quit), Shader/Texture/Camera/EarthSphere classes, GLSL with a simple sun.
Image loading via stb (FetchContent). Missing texture falls back to a magenta
checker. Drop `assets/textures/earth_day.jpg` (NASA Blue Marble) for the real map.

**Phase 2 — Data + propagation**
- Step 1 ✅ **DONE (2026-11-09):** libsgp4 (sources compiled directly),
  SatelliteCatalog (parse TLE + SGP4 to UTC now), CoordTransforms
  (ECI→geodetic→unit globe; theta = π − lon to match the flipped texture UVs),
  MarkerBatch (amber GL_POINTS), bundled real TLE snapshots (77 sats). Verified
  ISS altitude correct. Markers placed at true positions each frame.
- Step 2 ✅ **DONE (2026-11-09):** live CelesTrak fetch via cpr (libcurl built
  from source, Schannel TLS, no OpenSSL) + 6h disk cache in `./cache/`, bundled
  snapshot as offline fallback (`net/TleSource.cpp`, four-tier resolve). Starlink
  live feed capped to 60 to stay a curated sample. Tier-1 fetch logging (status,
  latency, curl error reason) to stderr. Verified live: dots load & move.

**Known issues / backlog (flagged during Phase 2, fix later):**
- **Blank white window until first data loads.** ✅ PARTLY RESOLVED (2026-11-11):
  a "Loading satellites..." frame is now drawn before the blocking load, so no
  white flash. Still synchronous on the main thread, so during a slow cold fetch
  the window is static/unresponsive. Optional upgrade (per §3.3): fetch on a
  background thread and hot-swap the catalog when it arrives.
- **Attribution + licenses (release blocker, Phase 6/7 README + About).** Two
  parts: (1) CelesTrak data credit — "Orbital data courtesy of CelesTrak
  (celestrak.org)", data originates from US Space Force / 18 SDS (already noted
  §8, §14); (2) third-party library licenses — required for binary distribution:
  GLFW (zlib), GLM (MIT), GLAD (MIT), stb (MIT/public domain), libsgp4
  (Apache-2.0), cpr + libcurl (MIT/curl). Ship a bundled THIRD-PARTY-LICENSES
  file + a credits section. Don't forget #2 — it's a legal requirement, not just
  courtesy. (3) **GeoNames city data — CC BY 4.0** (used for location search):
  must credit "GeoNames (geonames.org)" in About/credits + THIRD-PARTY-LICENSES.
- **High-DPI cursor scaling.** `pickSatellite` and the UI overlay work in
  framebuffer pixels, but `glfwGetCursorPos` returns window coords. Identical at
  100% display scaling; on a scaled/high-DPI monitor picking is off by the scale
  factor and the panel is sized in fb-pixels. Fix: convert cursor coords by
  framebuffer/window ratio (or do picking in NDC directly).
- **Marker style** ✅ RESOLVED (2026-11-11): colored per feed group (Stations
  amber / GPS green / Starlink orange-red) with a legend; selection cyan. Dot
  shape/size unchanged (round dots, 9px). Reopen only if per-object-type coloring
  is wanted later.
- **Track clarity pass** ✅ RESOLVED (2026-11-11): alpha-fade from "now" + orbit→
  shadow droplines added. (Shadow lift still 10 km; line width still 1px — tune
  only if needed.)
- **Diversify rendered feeds (deferred).** Current globe feeds (stations,
  gps-ops, starlink) are nearly all US payloads, so the info panel's Type/Owner
  are monotonous. Add more CelesTrak GROUPs in `main.cpp` feeds (each capped like
  Starlink) for variety: `glonass-ops` (Russia), `galileo` (ESA), `beidou`
  (China), `weather`/`noaa`/`goes` (mixed), and a debris group
  (`cosmos-2251-debris` / `iridium-33-debris`) to show non-payload object types.
  Data source stays CelesTrak — it already carries all of this keylessly; this is
  a feed-selection change, not a new provider. User: add in a later phase.
- **UI text is single-size, unhinted** (stb_truetype baked atlas, 18px, Segoe UI
  dev fallback). Upgrade to `stbtt_PackFontRange` + oversampling (or FreeType) if
  crispness isn't good enough; bundle the OFL font (see attribution item).
- ✅ **RESOLVED (2026-11-10): cache is now in a fixed per-user dir**
  (`%LOCALAPPDATA%\globe\cache\`, XDG/`~/.cache/globe` elsewhere) via
  `platform/Paths.cpp userCacheDir()`, no longer working-dir-relative. Note the
  Starlink cache file is ~1.8 MB (full feed) though we only render 60.
- **IP-based location auto-detect (deferred from Phase 5).** §7/§16.4 wanted both
  manual entry (done) and IP auto-detect on first run. Needs a keyless/low-
  friction geolocation endpoint (TBD) and an outbound call at startup — flag it
  to the user like the other network calls. Manual city/lat-lon entry covers the
  need for now.
- _(flag 2 — TBD)_

**Phase 3 — Live markers + selection + UI foundation** ✅ **DONE (2026-11-10)**
Live markers (Phase 2); **ray-pick selection** (screen-space nearest + ray-sphere
occlusion, click-vs-drag; `main.cpp pickSatellite`) with a cyan highlight; **first
custom UI panel** — from-scratch text rendering (stb_truetype glyph atlas →
`ui/TextRenderer`) + a minimal overlay toolkit (`ui/UiContext`: ortho rects +
text) showing the selected sat's live name/NORAD/lat/lon/alt.
**UI review checkpoint → DECIDED: stay custom.** Text rendering (the risky part)
works; we continue the bespoke toolkit for the list/search/time-control rather
than falling back to ImGui. Revisit only if a later widget proves too costly.

**Phase 4 — Info panel, ground tracks, list/search** ✅ **DONE (2026-11-10)**
- **Orbit path + ground shadow ✅ DONE (2026-11-10):** click a satellite → its
  path for one full orbital period (from mean motion), as GL_LINE_STRIP arcs
  (`render/TrackRenderer`, `track.vert/frag`). Two layers: the **orbit** at true
  altitude (through the marker — dim past arc, cyan future arc) and a **ground
  shadow**, the same points dropped to the surface (dark translucent line).
  Sampled from SGP4 each frame (so it slides with time), shadow lifted ~10 km to
  avoid z-fighting; the back half is hidden by the globe via depth test
  (`SatelliteCatalog::track` returns orbit+shadow past/future arcs).
- **Info panel: velocity + SATCAT ✅ DONE (2026-11-10):** live orbital speed
  (`SatelliteState.speedKmS` from `Eci::Velocity().Magnitude()`), plus object
  type / owner / launch date from CelesTrak SATCAT. `net/SatcatSource` fetches
  `satcat/records.php?GROUP=<g>&FORMAT=CSV` per feed group (weekly cache,
  best-effort — no bundled fallback, panel omits the fields if unavailable);
  `domain/Satcat` parses CSV by header name, keys by NORAD, humanizes owner/type
  codes. Panel height now fits its line count. First run on empty cache = 6
  network calls (3 TLE + 3 SATCAT).
- **Searchable list ✅ DONE (2026-11-10):** right-edge panel of the whole
  catalog (`ui/SatelliteList`). Type-to-filter by name/NORAD (search box with
  blinking caret + match count), wheel-scroll while hovering (rows clipped via
  new `UiContext::pushClip/popClip` scissor), click a row to select (same path as
  a marker pick). Tab toggles the panel; Esc clears the filter else quits. Text
  entry plumbed through `Window` (GLFW char + key(backspace) callbacks →
  `consumeTypedChars`/`consumeBackspaces`). Scroll/drag route to the list when the
  cursor is over it, else to the camera.

**Phase 5 — Location & pass predictions** ✅ **DONE (2026-11-11)**
_(IP-based auto-detect of location deferred — see backlog)_
- **Engine + location + overhead-now ✅ DONE (2026-11-10):** `SatelliteCatalog`
  gained `lookAngle()` (libsgp4 `Observer::GetLookAngle` → az/el/range) and
  `predictPasses()` (steps SGP4 forward ≤48h, bisection-refined horizon crossings
  → AOS/LOS/max-el, soonest first). Manual location entry via **F2** (type
  "lat, lon", Enter to set, parsed with `std::stod`); observer status shown
  bottom-left. Info panel shows Az/El/Range (up/below horizon) when a location is
  set. NOTE deviation from §5: look-angle/pass logic lives on SatelliteCatalog
  (SGP4 propagators are private there), not standalone Observer/PassPredictor.
  Location not persisted (Phase 6).
- **Location search (city gazetteer) ✅ DONE (2026-11-11):** F2 now opens a
  modal (`ui/LocationPicker`) that searches a bundled **GeoNames** city dump
  (`domain/Gazetteer` loads `assets/data/cities15000.txt`, name search ranked
  exact→prefix→substring then by population) — type a city, click a result, or
  type raw "lat, lon". Graceful fallback to lat/lon entry if the file is absent
  (not committed; see assets/data/README). GeoNames = CC BY (attribution added to
  backlog).
- **Satellite list minimizable ✅ DONE (2026-11-11):** the catalog panel has a
  header bar ("Satellites (N)") with a collapse control; click it to shrink to
  the bar, click to expand. Tab still fully hides it.
- **Pass display ✅ DONE (2026-11-11):** info panel shows the next ≤3 passes for
  the selected satellite from the observer — AOS countdown · duration · max
  elevation (in-progress pass shows "now LOS in …"; none → "No pass in 48h").
  Passes cached per selection, recomputed on change or every 5s; display counts
  down live off the cache. Threshold 10°.

**Phase 6 — Polish & release prep** _(in progress)_
- **Settings persistence ✅ DONE (2026-11-11):** `app/Settings` (new app/ layer)
  saves observer location + window size to a plain key=value file at
  `%LOCALAPPDATA%\globe\settings.cfg` (`platform/Paths::userConfigDir`), restored
  on launch. DEVIATION from §2 "locked SQLite": used a trivial key=value file per
  §11's "JSON if trivial" — settings are a handful of values; SQLite remains an
  option if structured data (favorites/history) is added later.
- **About / controls overlay ✅ DONE (2026-11-11):** F1 opens a centered overlay
  (`main.cpp`, static text) with version (`GLOBE_VERSION` from CMake), the full
  control list, and attributions (CelesTrak; GeoNames CC BY 4.0; GLFW/GLM/GLAD/
  stb/libsgp4/cpr+libcurl). Esc/F1/click closes. Bottom-left shows a key hint.
  In-app attribution now covered; the bundled THIRD-PARTY-LICENSES *file* is still
  a Phase 7 packaging task.
- **Visual polish ✅ DONE (2026-11-11):** (1) loading frame ("Loading
  satellites...") drawn before the blocking data load, killing the white flash
  (backlog flag 1 — cheap fix; bg-thread fetch still optional). (2) markers
  colored per feed group (Stations amber / GPS green / Starlink orange-red) with
  a bottom-left legend; selection stays cyan (resolves §6 marker-style review).
  (3) track clarity: orbit/shadow arcs alpha-fade from "now" (per-vertex alpha in
  TrackRenderer + track shader) + faint droplines connecting orbit to shadow
  (resolves the deferred track-clarity item).
- _Still to do:_ packaging (incl. THIRD-PARTY-LICENSES file) — that's Phase 7.
  Favorites/selection persistence optional.

**Phase 7 — Public release**
GitHub Release (Windows installer/zip), README with screenshots, optional code
signing, later a GitHub Pages download page.

---

## 14. Distribution & going public

- **Binaries → GitHub Releases** (free). Windows installer (Inno Setup/NSIS) or
  portable zip with bundled DLLs.
- **Download page (later) → GitHub Pages** — static site, download button links
  to the latest Release. No backend, no DB.
- **Code signing** ✅ **skipped for v1.** We accept the Windows SmartScreen
  "unknown publisher" warning rather than pay for a cert. The README download
  instructions will note it's expected. Revisit only if/when it becomes a real
  adoption barrier (or when/if we ship macOS, which needs Apple notarization).
- **No secrets in the binary** — v1 uses keyless CelesTrak, so nothing to leak.
- **CI (later):** GitHub Actions to build + attach artifacts on tag.

---

## 15. Explicitly deferred / future

- `RemoteProvider` (optional server for TLE caching / proxying) — only if a
  feature demands it; needs no real DB even then.
- Full ~10k active catalog as 3D markers (instanced rendering + LOD/culling).
- macOS / Linux builds.
- Night lights, cloud layer, atmosphere shader, specular oceans.
- Auto-update.
- Cloud DB — only if accounts / sync / server-side history are ever added.

---

## 16. Design questions — status

1. ✅ **Visible subset:** ISS/stations + GPS + Starlink sample on the globe;
   everything else in the searchable list. (Marker *style* still open.)
2. ✅ **TLE freshness:** 6h re-download window; positions always propagated to
   `now` and therefore always current — see §3.3.
3. ◻️ **UI path (conditional):** custom OpenGL UI is the primary path; **fall
   back to Dear ImGui** for panels if custom complexity isn't worth it. Decided
   at the **Phase 3 checkpoint**, globe stays custom either way.
4. ✅ **Location input:** both manual lat/lon **and** IP-based auto-detection.
   (Specific IP-geolocation endpoint TBD at Phase 5.)
5. ✅ **Code signing:** skipped for v1 (accept SmartScreen warning).
   ◻️ **Code license:** default **MIT** — confirm before first public release.
6. 🔵 **Earth texture resolution** vs download size — resolve at Phase 1
   (rendering).

Legend: ✅ resolved · ◻️ decided-with-condition / to confirm · 🔵 still open.

# Globe — Satellite Visualizer

A native (C++ / OpenGL) real-time satellite visualizer. It fetches orbital data
from a public source, propagates it locally, and renders satellites over a 3D
Earth — with info panels, ground tracks, and pass predictions based on your
location.

> **Status:** Phase 0 (scaffolding). Right now the app opens a window with an
> OpenGL 3.3 context and clears it each frame. See [`PLAN.md`](PLAN.md) for the
> full architecture and roadmap.

## Building

**Requirements**
- CMake 3.24+
- A C++20 compiler (MSVC 2022 / recent GCC / Clang)
- Python 3 (GLAD generates its loader at build time)
- Internet access on the **first** configure (CMake FetchContent downloads
  GLFW, GLM, and GLAD; they are cached in the build directory afterward)

**Windows (MSVC / Visual Studio generator)**
```sh
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
./build/Debug/globe.exe
```

**Ninja (if available, e.g. CLion's bundled toolchain)**
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/globe
```

Dependencies are fetched automatically — there is no separate install step.

## Controls (Phase 0)
- `Esc` — quit

## Data & attribution
- Orbital data (TLEs) and catalog metadata: **CelesTrak** (https://celestrak.org).
  Used under its fair-use terms; please do not poll aggressively.
- Earth imagery (planned): **NASA Visible Earth / Blue Marble** (public domain).

## License
TBD — intended to be MIT (see `PLAN.md` §12). Third-party libraries retain their
own licenses.

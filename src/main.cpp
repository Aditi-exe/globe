// Globe - Satellite Visualizer
// Phase 4: live markers + click-to-select + ground tracks. TLEs -> SGP4 ->
// lat/lon/alt -> globe markers; click a marker to select it and draw its ground
// track (one orbit of sub-satellite points, past + future arcs).
//   - Left-drag or arrow keys to rotate, scroll to zoom (eased).
//   - Left-click a satellite to select it, click empty space to clear. Esc quits.
// See PLAN.md for the roadmap.

#include "app/Settings.h"
#include "domain/Gazetteer.h"
#include "domain/Satcat.h"
#include "domain/SatelliteCatalog.h"
#include "net/SatcatSource.h"
#include "net/TleSource.h"
#include "platform/Paths.h"
#include "platform/Window.h"
#include "render/Camera.h"
#include "render/EarthSphere.h"
#include "render/MarkerBatch.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "render/TrackRenderer.h"
#include "ui/LocationPicker.h"
#include "ui/SatelliteList.h"
#include "ui/UiContext.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace {

// Screen-space pick: index into `states` of the nearest valid marker within
// `pixelThreshold` px of the cursor, or -1. Markers hidden behind the globe
// (the ray from the camera crosses the unit sphere before reaching them) are
// ignored, so you can only click what you can actually see.
int pickSatellite(const std::vector<globe::SatelliteState>& states,
                  const glm::vec3& camPos, const glm::mat4& view,
                  const glm::mat4& proj, double cursorX, double cursorY,
                  int width, int height, float pixelThreshold) {
    const glm::mat4 vp = proj * view;
    int best = -1;
    float bestDist = pixelThreshold;
    for (std::size_t i = 0; i < states.size(); ++i) {
        const globe::SatelliteState& s = states[i];
        if (!s.valid) {
            continue;
        }
        glm::vec4 clip = vp * glm::vec4(s.position, 1.0f);
        if (clip.w <= 0.0f) {
            continue; // behind the camera
        }
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
        float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);
        float dx = sx - static_cast<float>(cursorX);
        float dy = sy - static_cast<float>(cursorY);
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist >= bestDist) {
            continue;
        }
        // Occlusion: does the unit globe sit between the camera and this marker?
        glm::vec3 toP = s.position - camPos;
        float lenP = glm::length(toP);
        glm::vec3 dir = toP / lenP;
        float b = glm::dot(camPos, dir);
        float c = glm::dot(camPos, camPos) - 1.0f;
        float disc = b * b - c;
        if (disc > 0.0f) {
            float t = -b - std::sqrt(disc);
            if (t > 1e-3f && t < lenP - 1e-3f) {
                continue; // Earth blocks the line of sight
            }
        }
        best = static_cast<int>(i);
        bestDist = dist;
    }
    return best;
}

// Render a minute count as "5m" or "1h 03m".
std::string formatMinutes(double minutes) {
    if (minutes < 0.0) {
        minutes = 0.0;
    }
    const int total = static_cast<int>(minutes + 0.5);
    char b[32];
    if (total >= 60) {
        std::snprintf(b, sizeof(b), "%dh %02dm", total / 60, total % 60);
    } else {
        std::snprintf(b, sizeof(b), "%dm", total);
    }
    return b;
}

} // namespace

int main() {
    try {
        // Persistent settings (observer location, window size) in the per-user
        // config dir. Missing file = defaults.
        const std::string configDir = globe::userConfigDir("globe");
        const std::string settingsPath = configDir + "/settings.cfg";
        globe::Settings settings = globe::Settings::load(settingsPath);
        std::fprintf(stderr, "[settings] %s\n", settingsPath.c_str());

        globe::Window window(settings.windowWidth, settings.windowHeight,
                             "Globe - Satellite Visualizer");

        // Locate assets. In a packaged build they sit next to the executable
        // (assets/ beside globe.exe); for dev runs launched from the build dir
        // that folder won't exist, so fall back to the source tree baked in at
        // compile time (GLOBE_ASSETS_DIR).
        std::string assets = globe::exeDir() + "/assets";
        if (!std::filesystem::exists(std::filesystem::path(assets) / "shaders" / "earth.vert")) {
            assets = GLOBE_ASSETS_DIR;
        }
        std::fprintf(stderr, "[assets] %s\n", assets.c_str());
        globe::Shader earthShader(assets + "/shaders/earth.vert", assets + "/shaders/earth.frag");
        globe::Shader markerShader(assets + "/shaders/marker.vert", assets + "/shaders/marker.frag");
        globe::Shader trackShader(assets + "/shaders/track.vert", assets + "/shaders/track.frag");
        globe::Texture earthTexture(assets + "/textures/1280px-Blue_Marble_2002.png");
        globe::EarthSphere earth;
        globe::MarkerBatch markers;
        globe::MarkerBatch selectionMarker; // highlight for the selected satellite
        globe::TrackRenderer orbitPastTrack;    // orbit behind the selected sat
        globe::TrackRenderer orbitFutureTrack;  // orbit ahead of it
        globe::TrackRenderer shadowPastTrack;   // ground shadow of the past arc
        globe::TrackRenderer shadowFutureTrack; // ground shadow of the future arc
        globe::TrackRenderer dropLines;         // faint connectors orbit->shadow
        globe::Camera camera;
        globe::UiContext ui(assets);
        globe::SatelliteList satList; // searchable catalog panel (right edge)
        globe::LocationPicker picker; // F2 location chooser (city search / lat,lon)

        // Draw one frame before loading data, so the window shows a message
        // instead of a blank/white flash while the first fetch + parse runs.
        {
            glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            ui.begin(window.width(), window.height());
            const std::string msg = "Loading satellites...";
            ui.drawText((static_cast<float>(window.width()) - ui.textWidth(msg)) * 0.5f,
                        static_cast<float>(window.height()) * 0.5f - ui.lineHeight(),
                        msg, glm::vec4(0.9f, 0.93f, 1.0f, 1.0f));
            ui.end();
            window.swapBuffers();
            window.pollEvents();
        }

        // Live TLEs from CelesTrak with a 6h disk cache and bundled fallback.
        // Cache lives in a fixed per-user dir (not the working directory).
        // Starlink is capped so the live feed (thousands) stays a curated sample.
        const std::string cacheDir = globe::userCacheDir("globe");
        std::fprintf(stderr, "[cache] %s\n", cacheDir.c_str());
        const std::string gp = "https://celestrak.org/NORAD/elements/gp.php?GROUP=";
        struct Feed { globe::TleGroup group; int cap; glm::vec3 color; const char* label; };
        const std::vector<Feed> feeds = {
            {{"stations", gp + "stations&FORMAT=tle", assets + "/data/stations.tle"}, 0,
             {1.00f, 0.82f, 0.25f}, "Stations"},
            {{"gps-ops",  gp + "gps-ops&FORMAT=tle",  assets + "/data/gps-ops.tle"}, 0,
             {0.45f, 0.90f, 0.55f}, "GPS"},
            {{"starlink", gp + "starlink&FORMAT=tle", assets + "/data/starlink-sample.tle"}, 60,
             {1.00f, 0.50f, 0.35f}, "Starlink"},
        };

        globe::SatelliteCatalog catalog;
        int loaded = 0;
        for (std::size_t i = 0; i < feeds.size(); ++i) {
            loaded += catalog.loadTleFile(globe::resolveTleFile(feeds[i].group, cacheDir),
                                          feeds[i].cap, static_cast<int>(i));
        }
        std::fprintf(stderr, "[catalog] loaded %d satellites\n", loaded);

        // SATCAT metadata (country / type / launch date), one CSV per feed group,
        // keyed by NORAD id. Best-effort: if it can't be fetched, the panel just
        // omits those fields. Weekly cache (it changes slowly).
        globe::Satcat satcat;
        for (const auto& f : feeds) {
            satcat.loadCsv(globe::resolveSatcatFile(f.group.name, cacheDir));
        }
        std::fprintf(stderr, "[satcat] %zu records\n", satcat.size());

        // Gazetteer of world cities for the location search (bundled GeoNames
        // dump). Optional: if the file isn't present, the picker falls back to
        // manual lat/lon entry. See assets/data/README.
        globe::Gazetteer gazetteer;
        gazetteer.load(assets + "/data/cities15000.txt");
        picker.setGazetteer(&gazetteer);

        glEnable(GL_PROGRAM_POINT_SIZE);

        double lastX = 0.0;
        double lastY = 0.0;
        bool wasDragging = false;
        double pressX = 0.0;
        double pressY = 0.0;
        bool movedWhileDown = false;
        bool pressOverList = false; // true if the current drag started on the panel
        const double kClickSlop = 5.0; // px of motion still counted as a click
        int selectedIndex = -1;        // index into catalog.states(); -1 = none
        double lastTime = glfwGetTime();
        bool printedOnce = false;
        bool tabWasDown = false; // edge-detect Tab (toggle list) and Esc
        bool escWasDown = false;
        bool aboutOpen = false;  // F1 about/controls overlay
        bool f1WasDown = false;

        // Observer location, set via the F2 picker (city search or lat/lon).
        // IP auto-detect comes later.
        struct ObsLoc { double lat = 0.0, lon = 0.0, altKm = 0.0; bool set = false; };
        ObsLoc observer{settings.obsLat, settings.obsLon, settings.obsAltKm,
                        settings.hasObserver};
        if (observer.set) {
            std::fprintf(stderr, "[settings] observer restored: %.4f, %.4f\n",
                         observer.lat, observer.lon);
        }
        bool f2WasDown = false;
        bool enterWasDown = false;
        bool obsDirty = false;    // observer changed; forces a pass recompute

        // Cached pass predictions for the selected satellite (expensive, so
        // recomputed only on change or every few seconds; times are relative to
        // passesCalcTime so the panel can show a live countdown).
        std::vector<globe::Pass> passes;
        int passesIndex = -1;
        double passesCalcTime = 0.0;

        while (!window.shouldClose()) {
            window.pollEvents();
            GLFWwindow* w = window.handle();

            picker.setViewport(window.width(), window.height());

            // F1 toggles the About / controls overlay.
            bool f1Down = glfwGetKey(w, GLFW_KEY_F1) == GLFW_PRESS;
            if (f1Down && !f1WasDown) {
                aboutOpen = !aboutOpen;
            }
            f1WasDown = f1Down;

            // F2 opens/closes the location picker.
            bool f2Down = glfwGetKey(w, GLFW_KEY_F2) == GLFW_PRESS;
            if (f2Down && !f2WasDown) {
                picker.toggle();
            }
            f2WasDown = f2Down;

            // Tab toggles the catalog list (ignored while the picker is open).
            bool tabDown = glfwGetKey(w, GLFW_KEY_TAB) == GLFW_PRESS;
            if (tabDown && !tabWasDown && !picker.isOpen() && !aboutOpen) {
                satList.toggleVisible();
            }
            tabWasDown = tabDown;

            // Route typed characters: the picker takes priority, else the search
            // box (nothing while the About overlay is up).
            std::string typed = window.consumeTypedChars();
            int backspaces = window.consumeBackspaces();
            if (aboutOpen) {
                // consumed, ignored
            } else if (picker.isOpen()) {
                picker.appendText(typed);
                picker.backspace(backspaces);
            } else if (satList.visible()) {
                satList.appendText(typed);
                satList.backspace(backspaces);
            }
            picker.rebuild(); // refresh results when the query changed

            // Enter in the picker: use the typed "lat, lon", else the top match.
            bool enterDown = glfwGetKey(w, GLFW_KEY_ENTER) == GLFW_PRESS
                          || glfwGetKey(w, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
            if (enterDown && !enterWasDown && picker.isOpen()) {
                double la = 0.0;
                double lo = 0.0;
                if (picker.parseLatLon(la, lo)) {
                    observer = {la, lo, 0.0, true};
                    obsDirty = true;
                    picker.close();
                    std::fprintf(stderr, "[observer] %.4f, %.4f\n", la, lo);
                } else if (!picker.results().empty()) {
                    const globe::Place* p = picker.results().front();
                    observer = {p->lat, p->lon, 0.0, true};
                    obsDirty = true;
                    picker.close();
                    std::fprintf(stderr, "[observer] %s, %s  %.4f, %.4f\n",
                                 p->name.c_str(), p->country.c_str(), p->lat, p->lon);
                }
            }
            enterWasDown = enterDown;

            // Esc: close About, else the picker, else clear the filter, else quit.
            bool escDown = glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escDown && !escWasDown) {
                if (aboutOpen) {
                    aboutOpen = false;
                } else if (picker.isOpen()) {
                    picker.close();
                } else if (satList.visible() && !satList.filterEmpty()) {
                    satList.clearFilter();
                } else {
                    glfwSetWindowShouldClose(w, GLFW_TRUE);
                }
            }
            escWasDown = escDown;

            float dt = static_cast<float>(glfwGetTime() - lastTime);
            lastTime = glfwGetTime();

            const float keySpeed = 1.5f; // radians/second
            if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)  camera.orbit(keySpeed * dt, 0.0f);
            if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) camera.orbit(-keySpeed * dt, 0.0f);
            if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)    camera.orbit(0.0f, keySpeed * dt);
            if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)  camera.orbit(0.0f, -keySpeed * dt);

            double mx = 0.0;
            double my = 0.0;
            glfwGetCursorPos(w, &mx, &my);

            satList.setViewport(window.width(), window.height());
            const bool overList = satList.contains(mx, my);

            // Scroll wheel: the picker first, then the list, else zoom the globe.
            double scroll = window.consumeScrollDelta();
            if (scroll != 0.0) {
                if (picker.isOpen() && picker.contains(mx, my)) {
                    picker.scroll(scroll);
                } else if (overList) {
                    satList.scroll(scroll);
                } else {
                    camera.zoom(std::pow(0.9f, -static_cast<float>(scroll)));
                }
            }

            bool dragging = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (dragging && !wasDragging) { // press started
                pressX = mx;
                pressY = my;
                movedWhileDown = false;
                // A drag starting on a UI panel/modal never orbits the globe.
                pressOverList = overList || picker.isOpen();
            }
            if (dragging && wasDragging && !pressOverList) { // held over globe -> orbit
                camera.orbit(-static_cast<float>(mx - lastX) * 0.005f,
                             static_cast<float>(my - lastY) * 0.005f);
                double ddx = mx - pressX;
                double ddy = my - pressY;
                if (ddx * ddx + ddy * ddy > kClickSlop * kClickSlop) {
                    movedWhileDown = true;
                }
            }
            // Release without appreciable motion = a click (a pick), not a drag.
            bool pendingPick = (!dragging && wasDragging && !movedWhileDown);
            wasDragging = dragging;
            lastX = mx;
            lastY = my;

            camera.update(dt); // ease zoom toward its target

            // Propagate every satellite to the current time.
            catalog.updateNow();

            float aspect = (window.width() > 0 && window.height() > 0)
                               ? static_cast<float>(window.width()) / static_cast<float>(window.height())
                               : 1.0f;
            glm::mat4 view = camera.view();
            glm::mat4 proj = camera.projection(aspect);

            // Refresh the list's filtered view (needed for row hit-testing too).
            satList.rebuild(catalog.states());
            const int hoverRow = overList ? satList.rowAt(mx, my) : -1;

            // A click over the panel selects a row; otherwise it picks the
            // nearest unobscured marker (or clears the selection).
            if (pendingPick && aboutOpen) {
                aboutOpen = false; // any click dismisses the About overlay
            } else if (pendingPick && picker.isOpen()) {
                const int row = picker.rowAt(mx, my);
                if (row >= 0) {
                    const globe::Place* p = picker.results()[static_cast<std::size_t>(row)];
                    observer = {p->lat, p->lon, 0.0, true};
                    obsDirty = true;
                    picker.close();
                    std::fprintf(stderr, "[observer] %s, %s  %.4f, %.4f\n",
                                 p->name.c_str(), p->country.c_str(), p->lat, p->lon);
                } else if (!picker.contains(mx, my)) {
                    picker.close(); // click outside the modal dismisses it
                }
            } else if (pendingPick && satList.hitHeader(mx, my)) {
                satList.toggleMinimized(); // clicked the panel's collapse/expand bar
            } else if (pendingPick) {
                int hit = selectedIndex;
                if (overList) {
                    const int row = satList.rowAt(mx, my);
                    if (row >= 0) {
                        hit = row; // clicked a list row; empty panel space = no change
                    }
                } else {
                    hit = pickSatellite(catalog.states(), camera.position(), view, proj,
                                        mx, my, window.width(), window.height(), 14.0f);
                }
                if (hit != selectedIndex) {
                    selectedIndex = hit;
                    if (selectedIndex >= 0) {
                        const globe::SatelliteState& s =
                            catalog.states()[static_cast<std::size_t>(selectedIndex)];
                        std::fprintf(stderr,
                                     "[select] %s (NORAD %d) lat=%.2f lon=%.2f alt=%.1f km\n",
                                     s.name.c_str(), s.norad, s.latDeg, s.lonDeg, s.altKm);
                    } else {
                        std::fprintf(stderr, "[select] cleared\n");
                    }
                }
            }

            // Marker positions grouped by feed, for per-group coloring.
            std::vector<std::vector<glm::vec3>> groupPts(feeds.size());
            for (const auto& s : catalog.states()) {
                if (s.valid && s.group >= 0 && s.group < static_cast<int>(feeds.size())) {
                    groupPts[static_cast<std::size_t>(s.group)].push_back(s.position);
                }
            }

            std::vector<glm::vec3> selPts;
            if (selectedIndex >= 0) {
                const globe::SatelliteState& s =
                    catalog.states()[static_cast<std::size_t>(selectedIndex)];
                if (s.valid) {
                    selPts.push_back(s.position);
                }
            }
            selectionMarker.setPositions(selPts);

            // Track (orbit + ground shadow) for the selected satellite, rebuilt
            // each frame so it slides forward with time. Cheap: a few hundred
            // SGP4 steps for one satellite.
            if (selectedIndex >= 0) {
                globe::SatelliteTrack t =
                    catalog.track(static_cast<std::size_t>(selectedIndex));
                // Fade each arc out from "now": past arcs are bright at their end,
                // future arcs bright at their start.
                orbitPastTrack.setPointsFaded(t.orbitPast, false);
                orbitFutureTrack.setPointsFaded(t.orbitFuture, true);
                shadowPastTrack.setPointsFaded(t.shadowPast, false);
                shadowFutureTrack.setPointsFaded(t.shadowFuture, true);

                // Droplines: a few vertical connectors from orbit down to shadow.
                std::vector<glm::vec3> drops;
                const std::size_t stride = 12;
                for (std::size_t i = 0; i < t.orbitPast.size() && i < t.shadowPast.size();
                     i += stride) {
                    drops.push_back(t.orbitPast[i]);
                    drops.push_back(t.shadowPast[i]);
                }
                for (std::size_t i = 0; i < t.orbitFuture.size() && i < t.shadowFuture.size();
                     i += stride) {
                    drops.push_back(t.orbitFuture[i]);
                    drops.push_back(t.shadowFuture[i]);
                }
                dropLines.setPoints(drops);
            } else {
                orbitPastTrack.setPoints({});
                orbitFutureTrack.setPoints({});
                shadowPastTrack.setPoints({});
                shadowFutureTrack.setPoints({});
                dropLines.setPoints({});
            }

            // Pass predictions: recompute on selection/observer change, or every
            // 5s so finished passes roll off and the countdown stays honest.
            if (observer.set && selectedIndex >= 0) {
                const double nowT = glfwGetTime();
                if (selectedIndex != passesIndex || obsDirty ||
                    (nowT - passesCalcTime) > 5.0) {
                    passes = catalog.predictPasses(static_cast<std::size_t>(selectedIndex),
                                                   observer.lat, observer.lon,
                                                   observer.altKm, 3, 10.0);
                    passesIndex = selectedIndex;
                    passesCalcTime = nowT;
                    obsDirty = false;
                }
            } else {
                passes.clear();
                passesIndex = -1;
            }

            if (!printedOnce) {
                printedOnce = true;
                for (const auto& s : catalog.states()) {
                    if (s.norad == 25544) {
                        std::fprintf(stderr, "[verify] ISS lat=%.2f lon=%.2f alt=%.1f km\n",
                                     s.latDeg, s.lonDeg, s.altKm);
                    }
                }
            }

            glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            earthShader.use();
            earthShader.setMat4("uModel", glm::mat4(1.0f));
            earthShader.setMat4("uView", view);
            earthShader.setMat4("uProj", proj);
            earthShader.setInt("uTexture", 0);
            earthTexture.bind(0);
            earth.draw();

            // Orbit path + its ground shadow under the markers. Depth test (on)
            // hides the half behind the globe; blend softens the lines. Draw the
            // shadow first (it hugs the surface), then the orbit above it.
            if (selectedIndex >= 0) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                trackShader.use();
                trackShader.setMat4("uView", view);
                trackShader.setMat4("uProj", proj);

                // Ground shadow: dark line, faded from the sub-point.
                trackShader.setVec4("uColor", glm::vec4(0.02f, 0.03f, 0.05f, 0.6f));
                shadowPastTrack.draw();
                shadowFutureTrack.draw();

                // Droplines: faint vertical connectors tying orbit to its shadow.
                trackShader.setVec4("uColor", glm::vec4(0.5f, 0.6f, 0.72f, 0.22f));
                dropLines.drawLines();

                // Orbit path: dim where it's been, cyan where it's going (the
                // per-vertex alpha does the fade, so keep uColor.a high).
                trackShader.setVec4("uColor", glm::vec4(0.55f, 0.62f, 0.72f, 0.9f)); // past
                orbitPastTrack.draw();
                trackShader.setVec4("uColor", glm::vec4(0.35f, 0.85f, 1.0f, 1.0f)); // future
                orbitFutureTrack.draw();
                glDisable(GL_BLEND);
            }

            markerShader.use();
            markerShader.setMat4("uView", view);
            markerShader.setMat4("uProj", proj);
            markerShader.setFloat("uPointSize", 9.0f);
            for (std::size_t i = 0; i < feeds.size(); ++i) {
                markers.setPositions(groupPts[i]);
                markerShader.setVec3("uColor", feeds[i].color);
                markers.draw();
            }

            // Selected satellite: a larger cyan dot over its amber marker.
            markerShader.setFloat("uPointSize", 18.0f);
            markerShader.setVec3("uColor", glm::vec3(0.35f, 0.85f, 1.0f)); // cyan
            selectionMarker.draw();

            // 2D overlay: info panel for the selected satellite.
            ui.begin(window.width(), window.height());
            if (selectedIndex >= 0) {
                const globe::SatelliteState& s =
                    catalog.states()[static_cast<std::size_t>(selectedIndex)];

                const glm::vec4 white(0.92f, 0.95f, 1.0f, 1.0f);
                const glm::vec4 dim(0.60f, 0.68f, 0.82f, 1.0f);

                // Build the panel's lines first, so its height fits the content.
                std::vector<std::pair<std::string, glm::vec4>> lines;
                char buf[96];
                lines.emplace_back(s.name, white);
                std::snprintf(buf, sizeof(buf), "NORAD    %d", s.norad);
                lines.emplace_back(buf, dim);

                // SATCAT metadata, if we have a row for this object.
                if (const globe::SatcatInfo* info = satcat.find(s.norad)) {
                    std::snprintf(buf, sizeof(buf), "Type     %s",
                                  globe::objectTypeName(info->objectType).c_str());
                    lines.emplace_back(buf, dim);
                    std::snprintf(buf, sizeof(buf), "Owner    %s",
                                  globe::ownerName(info->ownerCode).c_str());
                    lines.emplace_back(buf, dim);
                    if (!info->launchDate.empty()) {
                        std::snprintf(buf, sizeof(buf), "Launched %s", info->launchDate.c_str());
                        lines.emplace_back(buf, dim);
                    }
                }

                std::snprintf(buf, sizeof(buf), "Lat      %.2f", s.latDeg);
                lines.emplace_back(buf, white);
                std::snprintf(buf, sizeof(buf), "Lon      %.2f", s.lonDeg);
                lines.emplace_back(buf, white);
                std::snprintf(buf, sizeof(buf), "Alt      %.1f km", s.altKm);
                lines.emplace_back(buf, white);
                std::snprintf(buf, sizeof(buf), "Speed    %.2f km/s", s.speedKmS);
                lines.emplace_back(buf, white);

                // From the observer's location (set via F2): where the satellite
                // is in the sky now, and when it next flies over.
                if (observer.set) {
                    globe::LookAngle la = catalog.lookAngle(
                        static_cast<std::size_t>(selectedIndex),
                        observer.lat, observer.lon, observer.altKm);
                    if (la.valid) {
                        lines.emplace_back("", dim); // spacer
                        lines.emplace_back("In your sky right now", dim);
                        std::snprintf(buf, sizeof(buf), "  Height above horizon  %.0f deg%s",
                                      la.elDeg, la.aboveHorizon ? "" : "  (below)");
                        lines.emplace_back(buf, la.aboveHorizon ? white : dim);
                        std::snprintf(buf, sizeof(buf), "  Compass direction    %.0f deg", la.azDeg);
                        lines.emplace_back(buf, dim);
                        std::snprintf(buf, sizeof(buf), "  Distance to it       %.0f km", la.rangeKm);
                        lines.emplace_back(buf, dim);
                    }

                    // Upcoming passes (live countdown off the cached prediction).
                    lines.emplace_back("", dim); // spacer
                    lines.emplace_back("Next fly-overs (reaches 10 deg up)", dim);
                    const double elapsedMin = (glfwGetTime() - passesCalcTime) / 60.0;
                    int shown = 0;
                    for (const globe::Pass& p : passes) {
                        const double aosIn = p.aosMinFromNow - elapsedMin;
                        const double losIn = p.losMinFromNow - elapsedMin;
                        if (losIn < -0.1) {
                            continue; // already ended since we computed
                        }
                        if (aosIn <= 0.0) {
                            std::snprintf(buf, sizeof(buf),
                                          "  overhead now, sets in %s (peak %.0f deg)",
                                          formatMinutes(losIn).c_str(), p.maxElDeg);
                        } else {
                            std::snprintf(buf, sizeof(buf),
                                          "  starts in %s, lasts %s, peak %.0f deg",
                                          formatMinutes(aosIn).c_str(),
                                          formatMinutes(p.durationMin).c_str(),
                                          p.maxElDeg);
                        }
                        lines.emplace_back(buf, white);
                        if (++shown >= 3) {
                            break;
                        }
                    }
                    if (shown == 0) {
                        lines.emplace_back("  none in the next 48 hours", dim);
                    }
                } else {
                    lines.emplace_back("", dim); // spacer
                    lines.emplace_back("Press F2 to set your location", dim);
                }

                const float pad = 12.0f;
                const float step = ui.lineHeight() + 5.0f;
                const float px = 16.0f;
                const float py = 16.0f;
                const float pw = 380.0f;
                const float ph = pad * 2.0f + step * static_cast<float>(lines.size());

                ui.drawRect(px, py, pw, ph, glm::vec4(0.04f, 0.06f, 0.10f, 0.82f));
                ui.drawRect(px, py, pw, 3.0f, glm::vec4(0.35f, 0.85f, 1.0f, 1.0f)); // accent

                float tx = px + pad;
                float ty = py + pad;
                for (const auto& ln : lines) {
                    ui.drawText(tx, ty, ln.first, ln.second);
                    ty += step;
                }
            }

            // Searchable catalog list on the right (Tab toggles it).
            satList.draw(ui, catalog.states(), selectedIndex, hoverRow, glfwGetTime());

            // Marker legend (bottom-left, above the status lines).
            {
                const glm::vec4 dimc(0.55f, 0.63f, 0.78f, 1.0f);
                const float lh = ui.lineHeight();
                const float ly = static_cast<float>(window.height()) - 3.0f * lh - 14.0f;
                const float sq = lh - 6.0f;
                float lx = 16.0f;
                for (const auto& f : feeds) {
                    ui.drawRect(lx, ly + 2.0f, sq, sq, glm::vec4(f.color, 1.0f));
                    const float labelX = lx + sq + 6.0f;
                    ui.drawText(labelX, ly, f.label, dimc);
                    lx = labelX + ui.textWidth(f.label) + 16.0f;
                }
            }

            // Observer status + key hint (bottom-left, two lines).
            {
                const glm::vec4 dimc(0.55f, 0.63f, 0.78f, 1.0f);
                const float lh = ui.lineHeight();
                const float h = static_cast<float>(window.height());
                ui.drawText(16.0f, h - 2.0f * lh - 12.0f,
                            "F1 help   F2 location   Tab list", dimc);
                char sbuf[96];
                if (observer.set) {
                    std::snprintf(sbuf, sizeof(sbuf), "Observer  %.2f, %.2f",
                                  observer.lat, observer.lon);
                } else {
                    std::snprintf(sbuf, sizeof(sbuf), "No location set");
                }
                ui.drawText(16.0f, h - lh - 10.0f, sbuf, dimc);
            }

            // Location picker modal (city search / lat,lon), drawn on top.
            picker.draw(ui, mx, my, glfwGetTime());

            // About / controls overlay (F1), topmost.
            if (aboutOpen) {
                const glm::vec4 bg(0.05f, 0.07f, 0.11f, 0.98f);
                const glm::vec4 accent(0.35f, 0.85f, 1.0f, 1.0f);
                const glm::vec4 white(0.92f, 0.95f, 1.0f, 1.0f);
                const glm::vec4 dim(0.60f, 0.68f, 0.82f, 1.0f);
                struct AboutLine { const char* text; bool header; };
                static const AboutLine about[] = {
                    {"Globe - Satellite Visualizer   v" GLOBE_VERSION, false},
                    {"", false},
                    {"Controls", true},
                    {"  Left-drag / arrow keys    rotate the globe", false},
                    {"  Scroll wheel              zoom in / out", false},
                    {"  Click a satellite         select it", false},
                    {"  Tab                       show / hide the list", false},
                    {"  F2                        set your location", false},
                    {"  F1 or Esc                 close this", false},
                    {"", false},
                    {"Data", true},
                    {"  Orbital data: CelesTrak (celestrak.org)", false},
                    {"  City data: GeoNames (geonames.org), CC BY 4.0", false},
                    {"", false},
                    {"Built with", true},
                    {"  GLFW, GLM, GLAD, stb, libsgp4, cpr + libcurl", false},
                    {"  Full terms: THIRD-PARTY-LICENSES", false},
                };
                const int n = static_cast<int>(sizeof(about) / sizeof(about[0]));
                const float step = ui.lineHeight() + 5.0f;
                const float padA = 18.0f;
                const float mw = 480.0f;
                const float mh = padA * 2.0f + step * static_cast<float>(n);
                const float mx0 = (static_cast<float>(window.width()) - mw) * 0.5f;
                const float my0 = (static_cast<float>(window.height()) - mh) * 0.5f;
                ui.drawRect(mx0, my0, mw, mh, bg);
                ui.drawRect(mx0, my0, mw, 3.0f, accent);
                float ty0 = my0 + padA;
                for (int i = 0; i < n; ++i) {
                    glm::vec4 c = dim;
                    if (about[i].header) {
                        c = accent;
                    } else if (i == 0) {
                        c = white;
                    }
                    ui.drawText(mx0 + padA, ty0, about[i].text, c);
                    ty0 += step;
                }
            }

            ui.end();

            window.swapBuffers();
        }

        // Persist settings on a clean exit.
        settings.hasObserver = observer.set;
        settings.obsLat = observer.lat;
        settings.obsLon = observer.lon;
        settings.obsAltKm = observer.altKm;
        settings.windowWidth = window.width();
        settings.windowHeight = window.height();
        settings.save(settingsPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}

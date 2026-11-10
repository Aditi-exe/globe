// Globe - Satellite Visualizer
// Phase 3: live markers + click-to-select. TLEs -> SGP4 -> lat/lon/alt -> globe
// markers; click a marker to select it (ray-pick). Next: a custom info panel.
//   - Left-drag or arrow keys to rotate, scroll to zoom (eased).
//   - Left-click a satellite to select it, click empty space to clear. Esc quits.
// See PLAN.md for the roadmap.

#include "domain/SatelliteCatalog.h"
#include "net/TleSource.h"
#include "platform/Paths.h"
#include "platform/Window.h"
#include "render/Camera.h"
#include "render/EarthSphere.h"
#include "render/MarkerBatch.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "ui/UiContext.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <string>
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

} // namespace

int main() {
    try {
        globe::Window window(1280, 720, "Globe - Satellite Visualizer");

        const std::string assets = GLOBE_ASSETS_DIR;
        globe::Shader earthShader(assets + "/shaders/earth.vert", assets + "/shaders/earth.frag");
        globe::Shader markerShader(assets + "/shaders/marker.vert", assets + "/shaders/marker.frag");
        globe::Texture earthTexture(assets + "/textures/1280px-Blue_Marble_2002.png");
        globe::EarthSphere earth;
        globe::MarkerBatch markers;
        globe::MarkerBatch selectionMarker; // highlight for the selected satellite
        globe::Camera camera;
        globe::UiContext ui(assets);

        // Live TLEs from CelesTrak with a 6h disk cache and bundled fallback.
        // Cache lives in a fixed per-user dir (not the working directory).
        // Starlink is capped so the live feed (thousands) stays a curated sample.
        const std::string cacheDir = globe::userCacheDir("globe");
        std::fprintf(stderr, "[cache] %s\n", cacheDir.c_str());
        const std::string gp = "https://celestrak.org/NORAD/elements/gp.php?GROUP=";
        struct Feed { globe::TleGroup group; int cap; };
        const std::vector<Feed> feeds = {
            {{"stations", gp + "stations&FORMAT=tle", assets + "/data/stations.tle"}, 0},
            {{"gps-ops",  gp + "gps-ops&FORMAT=tle",  assets + "/data/gps-ops.tle"}, 0},
            {{"starlink", gp + "starlink&FORMAT=tle", assets + "/data/starlink-sample.tle"}, 60},
        };

        globe::SatelliteCatalog catalog;
        int loaded = 0;
        for (const auto& f : feeds) {
            loaded += catalog.loadTleFile(globe::resolveTleFile(f.group, cacheDir), f.cap);
        }
        std::fprintf(stderr, "[catalog] loaded %d satellites\n", loaded);

        glEnable(GL_PROGRAM_POINT_SIZE);

        double lastX = 0.0;
        double lastY = 0.0;
        bool wasDragging = false;
        double pressX = 0.0;
        double pressY = 0.0;
        bool movedWhileDown = false;
        const double kClickSlop = 5.0; // px of motion still counted as a click
        int selectedIndex = -1;        // index into catalog.states(); -1 = none
        double lastTime = glfwGetTime();
        bool printedOnce = false;

        while (!window.shouldClose()) {
            window.pollEvents();
            GLFWwindow* w = window.handle();

            if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(w, GLFW_TRUE);
            }

            float dt = static_cast<float>(glfwGetTime() - lastTime);
            lastTime = glfwGetTime();

            const float keySpeed = 1.5f; // radians/second
            if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)  camera.orbit(keySpeed * dt, 0.0f);
            if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) camera.orbit(-keySpeed * dt, 0.0f);
            if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)    camera.orbit(0.0f, keySpeed * dt);
            if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)  camera.orbit(0.0f, -keySpeed * dt);

            double scroll = window.consumeScrollDelta();
            if (scroll != 0.0) {
                camera.zoom(std::pow(0.9f, -static_cast<float>(scroll)));
            }

            double mx = 0.0;
            double my = 0.0;
            glfwGetCursorPos(w, &mx, &my);
            bool dragging = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (dragging && !wasDragging) { // press started
                pressX = mx;
                pressY = my;
                movedWhileDown = false;
            }
            if (dragging && wasDragging) { // held -> orbit
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

            // A click selects the nearest unobscured marker (or clears selection).
            if (pendingPick) {
                int hit = pickSatellite(catalog.states(), camera.position(), view, proj,
                                        mx, my, window.width(), window.height(), 14.0f);
                if (hit != selectedIndex) {
                    selectedIndex = hit;
                    if (hit >= 0) {
                        const globe::SatelliteState& s =
                            catalog.states()[static_cast<std::size_t>(hit)];
                        std::fprintf(stderr,
                                     "[select] %s (NORAD %d) lat=%.2f lon=%.2f alt=%.1f km\n",
                                     s.name.c_str(), s.norad, s.latDeg, s.lonDeg, s.altKm);
                    } else {
                        std::fprintf(stderr, "[select] cleared\n");
                    }
                }
            }

            // Marker positions for this frame.
            std::vector<glm::vec3> pts;
            pts.reserve(catalog.size());
            for (const auto& s : catalog.states()) {
                if (s.valid) {
                    pts.push_back(s.position);
                }
            }
            markers.setPositions(pts);

            std::vector<glm::vec3> selPts;
            if (selectedIndex >= 0) {
                const globe::SatelliteState& s =
                    catalog.states()[static_cast<std::size_t>(selectedIndex)];
                if (s.valid) {
                    selPts.push_back(s.position);
                }
            }
            selectionMarker.setPositions(selPts);

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

            markerShader.use();
            markerShader.setMat4("uView", view);
            markerShader.setMat4("uProj", proj);
            markerShader.setFloat("uPointSize", 9.0f);
            markerShader.setVec3("uColor", glm::vec3(1.0f, 0.85f, 0.2f)); // amber
            markers.draw();

            // Selected satellite: a larger cyan dot over its amber marker.
            markerShader.setFloat("uPointSize", 18.0f);
            markerShader.setVec3("uColor", glm::vec3(0.35f, 0.85f, 1.0f)); // cyan
            selectionMarker.draw();

            // 2D overlay: info panel for the selected satellite.
            ui.begin(window.width(), window.height());
            if (selectedIndex >= 0) {
                const globe::SatelliteState& s =
                    catalog.states()[static_cast<std::size_t>(selectedIndex)];
                const float pad = 12.0f;
                const float step = ui.lineHeight() + 5.0f;
                const float px = 16.0f;
                const float py = 16.0f;
                const float pw = 280.0f;
                const float ph = pad * 2.0f + step * 5.0f;

                ui.drawRect(px, py, pw, ph, glm::vec4(0.04f, 0.06f, 0.10f, 0.82f));
                ui.drawRect(px, py, pw, 3.0f, glm::vec4(0.35f, 0.85f, 1.0f, 1.0f)); // accent

                const glm::vec4 white(0.92f, 0.95f, 1.0f, 1.0f);
                const glm::vec4 dim(0.60f, 0.68f, 0.82f, 1.0f);
                float tx = px + pad;
                float ty = py + pad;
                char buf[96];

                ui.drawText(tx, ty, s.name, white);
                ty += step;
                std::snprintf(buf, sizeof(buf), "NORAD  %d", s.norad);
                ui.drawText(tx, ty, buf, dim);
                ty += step;
                std::snprintf(buf, sizeof(buf), "Lat    %.2f", s.latDeg);
                ui.drawText(tx, ty, buf, white);
                ty += step;
                std::snprintf(buf, sizeof(buf), "Lon    %.2f", s.lonDeg);
                ui.drawText(tx, ty, buf, white);
                ty += step;
                std::snprintf(buf, sizeof(buf), "Alt    %.1f km", s.altKm);
                ui.drawText(tx, ty, buf, white);
            }
            ui.end();

            window.swapBuffers();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}

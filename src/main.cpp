// Globe - Satellite Visualizer
// Phase 2: live-propagated satellites plotted at their true positions over the
// Earth. TLEs (bundled snapshot for now) -> SGP4 -> lat/lon/alt -> globe markers.
//   - Left-drag or arrow keys to rotate, scroll to zoom (eased), Esc to quit.
// See PLAN.md for the roadmap.

#include "domain/SatelliteCatalog.h"
#include "net/TleSource.h"
#include "platform/Window.h"
#include "render/Camera.h"
#include "render/EarthSphere.h"
#include "render/MarkerBatch.h"
#include "render/Shader.h"
#include "render/Texture.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    try {
        globe::Window window(1280, 720, "Globe - Satellite Visualizer");

        const std::string assets = GLOBE_ASSETS_DIR;
        globe::Shader earthShader(assets + "/shaders/earth.vert", assets + "/shaders/earth.frag");
        globe::Shader markerShader(assets + "/shaders/marker.vert", assets + "/shaders/marker.frag");
        globe::Texture earthTexture(assets + "/textures/1280px-Blue_Marble_2002.png");
        globe::EarthSphere earth;
        globe::MarkerBatch markers;
        globe::Camera camera;

        // Live TLEs from CelesTrak with a 6h disk cache and bundled fallback.
        // Starlink is capped so the live feed (thousands) stays a curated sample.
        const std::string cacheDir =
            (std::filesystem::current_path() / "cache").string();
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
            if (dragging && wasDragging) {
                camera.orbit(-static_cast<float>(mx - lastX) * 0.005f,
                             static_cast<float>(my - lastY) * 0.005f);
            }
            wasDragging = dragging;
            lastX = mx;
            lastY = my;

            camera.update(dt); // ease zoom toward its target

            // Propagate to the current time and collect valid marker positions.
            catalog.updateNow();
            std::vector<glm::vec3> pts;
            pts.reserve(catalog.size());
            for (const auto& s : catalog.states()) {
                if (s.valid) {
                    pts.push_back(s.position);
                }
            }
            markers.setPositions(pts);

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

            float aspect = (window.width() > 0 && window.height() > 0)
                               ? static_cast<float>(window.width()) / static_cast<float>(window.height())
                               : 1.0f;
            glm::mat4 view = camera.view();
            glm::mat4 proj = camera.projection(aspect);

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
            markers.draw();

            window.swapBuffers();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}

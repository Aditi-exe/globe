// Globe - Satellite Visualizer
// Phase 1: a textured Earth sphere with an orbit/trackball camera.
//   - Left-drag or arrow keys to rotate, scroll to zoom, Esc to quit.
//   - Drop a NASA Blue Marble image at assets/textures/earth_day.jpg; without
//     it the globe shows a magenta placeholder checker.
// (Satellite propagation/markers are parked in parked/phase2/ for later.)
// See PLAN.md for the roadmap.

#include "platform/Window.h"
#include "render/Camera.h"
#include "render/EarthSphere.h"
#include "render/Shader.h"
#include "render/Texture.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>
#include <exception>
#include <string>

int main() {
    try {
        globe::Window window(1280, 720, "Globe - Satellite Visualizer");

        const std::string assets = GLOBE_ASSETS_DIR;
        globe::Shader shader(assets + "/shaders/earth.vert", assets + "/shaders/earth.frag");
        globe::Texture earthTexture(assets + "/textures/1280px-Blue_Marble_2002.png");
        globe::EarthSphere earth;
        globe::Camera camera;

        double lastX = 0.0;
        double lastY = 0.0;
        bool wasDragging = false;
        double lastTime = glfwGetTime();

        while (!window.shouldClose()) {
            window.pollEvents();
            GLFWwindow* w = window.handle();

            if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(w, GLFW_TRUE);
            }

            // Frame delta, used for time-based (frame-rate independent) key input.
            float dt = static_cast<float>(glfwGetTime() - lastTime);
            lastTime = glfwGetTime();

            // Arrow keys orbit the camera (same sense as dragging).
            const float keySpeed = 1.5f; // radians/second
            if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)  camera.orbit(keySpeed * dt, 0.0f);
            if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) camera.orbit(-keySpeed * dt, 0.0f);
            if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)    camera.orbit(0.0f, keySpeed * dt);
            if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)  camera.orbit(0.0f, -keySpeed * dt);

            // Zoom: scroll up -> zoom out. pow() gives smooth multiplicative zoom.
            double scroll = window.consumeScrollDelta();
            if (scroll != 0.0) {
                camera.zoom(std::pow(0.9f, -static_cast<float>(scroll)));
            }

            // Orbit: while the left button is held, drag deltas rotate the view.
            double mx = 0.0;
            double my = 0.0;
            glfwGetCursorPos(w, &mx, &my);
            bool dragging = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (dragging && wasDragging) {
                float dx = static_cast<float>(mx - lastX);
                float dy = static_cast<float>(my - lastY);
                camera.orbit(-dx * 0.005f, dy * 0.005f);
            }
            wasDragging = dragging;
            lastX = mx;
            lastY = my;

            camera.update(dt); // ease zoom toward its target

            glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float aspect = (window.width() > 0 && window.height() > 0)
                               ? static_cast<float>(window.width()) / static_cast<float>(window.height())
                               : 1.0f;

            shader.use();
            shader.setMat4("uModel", glm::mat4(1.0f));
            shader.setMat4("uView", camera.view());
            shader.setMat4("uProj", camera.projection(aspect));
            shader.setInt("uTexture", 0);
            earthTexture.bind(0);
            earth.draw();

            window.swapBuffers();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}

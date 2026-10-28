// Globe - Satellite Visualizer
// Phase 0: open a window with an OpenGL 3.3 core context and clear it each
// frame. No scene, data, or UI yet - this is the foundation everything else
// builds on. See PLAN.md for the roadmap.

#include "platform/Window.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <exception>

int main() {
    try {
        globe::Window window(1280, 720, "Globe - Satellite Visualizer");

        while (!window.shouldClose()) {
            window.pollEvents();

            // Esc to quit (temporary convenience for Phase 0).
            if (glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window.handle(), GLFW_TRUE);
            }

            glClearColor(0.02f, 0.03f, 0.06f, 1.0f); // deep space blue
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            window.swapBuffers();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}

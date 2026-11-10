#include "platform/Window.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace globe {

namespace {

void framebufferSizeCallback(GLFWwindow* handle, int width, int height) {
    glViewport(0, 0, width, height);
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(handle))) {
        self->onResize(width, height);
    }
}

void scrollCallback(GLFWwindow* handle, double /*xOffset*/, double yOffset) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(handle))) {
        self->onScroll(yOffset);
    }
}

void charCallback(GLFWwindow* handle, unsigned int codepoint) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(handle))) {
        self->onChar(codepoint);
    }
}

void keyCallback(GLFWwindow* handle, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_BACKSPACE && action != GLFW_RELEASE) { // press + repeat
        if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(handle))) {
            self->onBackspace();
        }
    }
}

void errorCallback(int code, const char* description) {
    // Phase 0: surface GLFW errors to stderr. Swap for a real logger later.
    std::fprintf(stderr, "[GLFW] error %d: %s\n", code, description);
}

} // namespace

Window::Window(int width, int height, std::string title)
    : width_(width), height_(height), title_(std::move(title)) {
    glfwSetErrorCallback(errorCallback);

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // required on macOS
    glfwWindowHint(GLFW_SAMPLES, 4);                       // 4x MSAA

    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetCharCallback(window_, charCallback);
    glfwSetKeyCallback(window_, keyCallback);

    if (!gladLoadGL(glfwGetProcAddress)) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("Failed to load OpenGL functions via GLAD");
    }

    glfwSwapInterval(1); // vsync on

    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_) != 0;
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::swapBuffers() const {
    glfwSwapBuffers(window_);
}

void Window::onResize(int width, int height) {
    width_ = width;
    height_ = height;
}

void Window::onScroll(double yOffset) {
    scrollDelta_ += yOffset;
}

void Window::onChar(unsigned int codepoint) {
    // Our glyph atlas is printable ASCII (32..126); ignore anything else.
    if (codepoint >= 32 && codepoint < 127) {
        typed_ += static_cast<char>(codepoint);
    }
}

void Window::onBackspace() {
    ++backspaces_;
}

double Window::consumeScrollDelta() {
    double d = scrollDelta_;
    scrollDelta_ = 0.0;
    return d;
}

std::string Window::consumeTypedChars() {
    std::string s = typed_;
    typed_.clear();
    return s;
}

int Window::consumeBackspaces() {
    int n = backspaces_;
    backspaces_ = 0;
    return n;
}

} // namespace globe

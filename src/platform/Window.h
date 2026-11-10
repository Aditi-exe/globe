#pragma once

#include <string>

struct GLFWwindow;

namespace globe {

// Owns a GLFW window and its OpenGL 3.3 core context. RAII: construction opens
// the window and loads GL; destruction tears everything down. Non-copyable,
// non-movable (it owns a raw GLFW handle and registers itself as the window's
// user pointer).
class Window {
public:
    Window(int width, int height, std::string title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool shouldClose() const;
    void pollEvents() const;
    void swapBuffers() const;

    // Accumulated mouse-scroll offset since the last call (used for zoom).
    [[nodiscard]] double consumeScrollDelta();

    // Text-entry queue for the search box: printable characters typed since the
    // last call, and the number of backspaces pressed (repeats included).
    [[nodiscard]] std::string consumeTypedChars();
    [[nodiscard]] int consumeBackspaces();

    // Called by GLFW callbacks; keep cached state in sync.
    void onResize(int width, int height);
    void onScroll(double yOffset);
    void onChar(unsigned int codepoint);
    void onBackspace();

    [[nodiscard]] GLFWwindow* handle() const { return window_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

private:
    GLFWwindow* window_ = nullptr;
    int width_;
    int height_;
    std::string title_;
    double scrollDelta_ = 0.0;
    std::string typed_;      // printable chars queued since last consume
    int backspaces_ = 0;     // backspace presses queued since last consume
};

} // namespace globe

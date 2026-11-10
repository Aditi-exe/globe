#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace globe {

class Shader;
class TextRenderer;

// Minimal immediate-mode-style 2D overlay: filled rects + text in pixel space.
// Call begin() once per frame (sets up an ortho projection and blend state),
// issue drawRect/drawText, then end() to restore 3D render state. Scoped to the
// handful of widgets this app needs (§8), not a general UI framework.
class UiContext {
public:
    explicit UiContext(const std::string& assetsDir);
    ~UiContext();

    UiContext(const UiContext&) = delete;
    UiContext& operator=(const UiContext&) = delete;

    void begin(int width, int height); // ortho for w x h, blend on, depth off
    void end();                        // restore depth test

    void drawRect(float x, float y, float w, float h, const glm::vec4& color);
    void drawText(float x, float y, const std::string& text, const glm::vec4& color);

    [[nodiscard]] float textWidth(const std::string& text) const;
    [[nodiscard]] float lineHeight() const;
    [[nodiscard]] bool textReady() const;

private:
    glm::mat4 proj_{1.0f};
    std::unique_ptr<Shader> rectShader_;
    std::unique_ptr<TextRenderer> text_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};

} // namespace globe

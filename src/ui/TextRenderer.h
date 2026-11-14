#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace globe {

// Bakes a single-size ASCII glyph atlas from a .ttf (stb_truetype) and draws
// strings as textured quads in an orthographic (pixel) space. First-cut custom
// text: one font, one size, ASCII 32..126. Upgrade to packed/multi-size later.
class TextRenderer {
public:
    // Loads assetsDir/fonts/ui.ttf, falling back to a few system fonts for dev.
    // ready() is false if no usable font was found (draw/measure become no-ops).
    explicit TextRenderer(const std::string& assetsDir, float pixelHeight = 18.0f);
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    [[nodiscard]] bool ready() const;
    [[nodiscard]] float lineHeight() const;
    [[nodiscard]] float measure(const std::string& text) const; // pixel width

    // Draw `text` with its top-left at (x, y) pixels, using the given ortho proj.
    // `scale` enlarges (or shrinks) the baked glyphs about that top-left corner;
    // 1.0 is the native baked size. Scaling up is bilinear-filtered, so keep it
    // modest for crisp text.
    void draw(const glm::mat4& proj, float x, float y,
              const std::string& text, const glm::vec4& color, float scale = 1.0f) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace globe

#pragma once

#include <string>

namespace globe {

// A 2D OpenGL texture loaded from an image file. If the file is missing or
// fails to decode, falls back to a generated magenta/black checkerboard so the
// app still runs (and the missing texture is visually obvious).
class Texture {
public:
    explicit Texture(const std::string& path);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind(unsigned int unit = 0) const;

    [[nodiscard]] unsigned int id() const { return id_; }

private:
    unsigned int id_ = 0;
};

} // namespace globe

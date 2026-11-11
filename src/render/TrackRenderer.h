#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace globe {

// Draws a dynamic polyline (a satellite track) with a per-vertex alpha, so an
// arc can fade out toward one end. Also draws disconnected segments (droplines).
class TrackRenderer {
public:
    TrackRenderer();
    ~TrackRenderer();

    TrackRenderer(const TrackRenderer&) = delete;
    TrackRenderer& operator=(const TrackRenderer&) = delete;

    // Opaque: alpha = 1 per vertex (final alpha still modulated by uColor.a).
    void setPoints(const std::vector<glm::vec3>& points);

    // Alpha ramp: full at the "now" end, fading toward the far end.
    // brightAtStart = true means points[0] is the now end.
    void setPointsFaded(const std::vector<glm::vec3>& points, bool brightAtStart);

    void draw() const;      // GL_LINE_STRIP (a connected path)
    void drawLines() const; // GL_LINES (point pairs = separate segments)

private:
    void upload(const std::vector<float>& verts);

    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int count_ = 0; // vertex count
};

} // namespace globe

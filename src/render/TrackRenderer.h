#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace globe {

// Draws a dynamic polyline (a satellite ground track) as a GL_LINE_STRIP.
// Same shape as MarkerBatch, but connected line segments instead of points.
class TrackRenderer {
public:
    TrackRenderer();
    ~TrackRenderer();

    TrackRenderer(const TrackRenderer&) = delete;
    TrackRenderer& operator=(const TrackRenderer&) = delete;

    void setPoints(const std::vector<glm::vec3>& points);
    void draw() const;

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int count_ = 0;
};

} // namespace globe

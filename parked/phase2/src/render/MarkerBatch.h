#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace globe {

// Draws a dynamic set of points (satellite markers) as GL_POINTS.
class MarkerBatch {
public:
    MarkerBatch();
    ~MarkerBatch();

    MarkerBatch(const MarkerBatch&) = delete;
    MarkerBatch& operator=(const MarkerBatch&) = delete;

    void setPositions(const std::vector<glm::vec3>& points);
    void draw() const;

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int count_ = 0;
};

} // namespace globe

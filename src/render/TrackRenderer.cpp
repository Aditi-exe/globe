#include "render/TrackRenderer.h"

#include <glad/gl.h>

namespace globe {

namespace {
constexpr float kMinAlpha = 0.12f; // alpha at the far end of a faded arc
} // namespace

TrackRenderer::TrackRenderer() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // Interleaved: vec3 position, float alpha.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

TrackRenderer::~TrackRenderer() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
}

void TrackRenderer::upload(const std::vector<float>& verts) {
    count_ = static_cast<int>(verts.size() / 4);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);
}

void TrackRenderer::setPoints(const std::vector<glm::vec3>& points) {
    std::vector<float> v;
    v.reserve(points.size() * 4);
    for (const glm::vec3& p : points) {
        v.push_back(p.x);
        v.push_back(p.y);
        v.push_back(p.z);
        v.push_back(1.0f);
    }
    upload(v);
}

void TrackRenderer::setPointsFaded(const std::vector<glm::vec3>& points, bool brightAtStart) {
    std::vector<float> v;
    v.reserve(points.size() * 4);
    const std::size_t n = points.size();
    for (std::size_t i = 0; i < n; ++i) {
        const float t = (n <= 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(n - 1);
        const float nearness = brightAtStart ? (1.0f - t) : t; // 1 at the now end
        const float a = kMinAlpha + (1.0f - kMinAlpha) * nearness;
        v.push_back(points[i].x);
        v.push_back(points[i].y);
        v.push_back(points[i].z);
        v.push_back(a);
    }
    upload(v);
}

void TrackRenderer::draw() const {
    if (count_ < 2) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINE_STRIP, 0, count_);
    glBindVertexArray(0);
}

void TrackRenderer::drawLines() const {
    if (count_ < 2) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, count_);
    glBindVertexArray(0);
}

} // namespace globe
